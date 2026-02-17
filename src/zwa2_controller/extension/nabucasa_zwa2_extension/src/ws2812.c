/*
 * ws2812.c
 *
 * WS2812 addressable RGB LED driver using raw EUSART + LDMA
 *
 * Uses EUSART in SPI master mode to bit-bang the WS2812 protocol.
 * Each WS2812 bit is encoded as 4 SPI bits at 3.2MHz:
 *   0 → 0b1000 (one high bit, three low)
 *   1 → 0b1110 (three high bits, one low)
 *
 * LDMA fires a single memory-to-peripheral transfer per refresh.
 */

#include "ws2812.h"
#include <string.h>
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_eusart.h"
#include "em_ldma.h"

// LDMA channel for SPI TX (must not collide with Z-Wave PAL channels)
#define TX_LDMA_CHANNEL  1

// 4 SPI bits per WS2812 bit at 3.2 MHz → 312.5 ns per SPI bit
#define WS2812_SPI_BITRATE  3200000

#define WS2812_BIT_0  0b1000
#define WS2812_BIT_1  0b1110
#define WS2812_BITS   4

#define RESET_SIGNAL_BYTES     90
#define SPI_BUFFER_SIZE_BYTES  (RESET_SIGNAL_BYTES + (3 * (WS2812_NUM_LEDS * WS2812_BITS)))

static uint8_t spi_tx_buffer[SPI_BUFFER_SIZE_BYTES] = {0};

// Lookup table: for each possible byte value, the 4-byte SPI encoding
static uint8_t ws2812_lookup[256][4];

// LDMA descriptor and transfer config
static LDMA_Descriptor_t ldma_tx_descriptor;
static LDMA_TransferCfg_t ldma_tx_config;

// Precompute SPI bit patterns for all 256 possible byte values
static void precompute_ws2812_patterns(void)
{
    for (int i = 0; i < 256; i++) {
        for (int byte = 0; byte < 4; byte++) {
            int bit_h = 7 - (2 * byte);
            int bit_l = bit_h - 1;

            uint8_t code_h = (i & (1 << bit_h)) ? WS2812_BIT_1 : WS2812_BIT_0;
            uint8_t code_l = (i & (1 << bit_l)) ? WS2812_BIT_1 : WS2812_BIT_0;

            ws2812_lookup[i][byte] = (code_h << 4) | (code_l << 0);
        }
    }
}

// Internal context type
typedef struct {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    sl_led_state_t state;
} ws2812_context_t;

static ws2812_context_t ws2812_context = {
    .red = 19275,
    .green = 19275,
    .blue = 19275,
    .state = SL_LED_CURRENT_STATE_OFF
};

static void ws2812_send_buffer(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t *spi_ptr = &spi_tx_buffer[RESET_SIGNAL_BYTES];

    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        memcpy(spi_ptr, ws2812_lookup[g], 4);
        spi_ptr += 4;
        memcpy(spi_ptr, ws2812_lookup[r], 4);
        spi_ptr += 4;
        memcpy(spi_ptr, ws2812_lookup[b], 4);
        spi_ptr += 4;
    }

    LDMA_StartTransfer(TX_LDMA_CHANNEL, &ldma_tx_config, &ldma_tx_descriptor);
}

static void initEUSART(void)
{
    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockEnable(cmuClock_EUSART1, true);

    // Configure MOSI (TX) and SCLK pins as outputs
    GPIO_PinModeSet(WS2812_SPI_TX_PORT, WS2812_SPI_TX_PIN, gpioModePushPull, 1);
    GPIO_PinModeSet(WS2812_SPI_SCLK_PORT, WS2812_SPI_SCLK_PIN, gpioModePushPull, 1);

    // Route EUSART TX and SCLK to the configured pins
    GPIO->EUSARTROUTE[WS2812_SPI_PERIPHERAL_NO].TXROUTE =
        (WS2812_SPI_TX_PORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
        | (WS2812_SPI_TX_PIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
    GPIO->EUSARTROUTE[WS2812_SPI_PERIPHERAL_NO].SCLKROUTE =
        (WS2812_SPI_SCLK_PORT << _GPIO_EUSART_SCLKROUTE_PORT_SHIFT)
        | (WS2812_SPI_SCLK_PIN << _GPIO_EUSART_SCLKROUTE_PIN_SHIFT);
    GPIO->EUSARTROUTE[WS2812_SPI_PERIPHERAL_NO].ROUTEEN =
        GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_SCLKPEN;

    // Configure EUSART as SPI master, MSB first
    EUSART_SpiAdvancedInit_TypeDef adv = EUSART_SPI_ADVANCED_INIT_DEFAULT;
    adv.msbFirst = true;

    EUSART_SpiInit_TypeDef init = EUSART_SPI_MASTER_INIT_DEFAULT_HF;
    init.bitRate = WS2812_SPI_BITRATE;
    init.advancedSettings = &adv;

    EUSART_SpiInit(WS2812_SPI_PERIPHERAL, &init);
}

static void initLDMA(void)
{
    LDMA_Init_t ldmaInit = LDMA_INIT_DEFAULT;
    LDMA_Init(&ldmaInit);

    ldma_tx_descriptor = (LDMA_Descriptor_t)LDMA_DESCRIPTOR_SINGLE_M2P_BYTE(
        spi_tx_buffer, &(WS2812_SPI_PERIPHERAL->TXDATA), SPI_BUFFER_SIZE_BYTES);

    ldma_tx_config = (LDMA_TransferCfg_t)LDMA_TRANSFER_CFG_PERIPHERAL(
        ldmaPeripheralSignal_EUSART1_TXFL);
}

static sl_status_t ws2812_led_init(void *context)
{
    (void)context;
    GPIO_PinModeSet(WS2812_EN_PORT, WS2812_EN_PIN, gpioModePushPull, 1);
    initEUSART();
    initLDMA();
    return SL_STATUS_OK;
}

static void ws2812_led_turn_on(void *context)
{
    ws2812_context_t *ctx = (ws2812_context_t *)context;
    ctx->state = SL_LED_CURRENT_STATE_ON;
}

static void ws2812_led_turn_off(void *context)
{
    ws2812_context_t *ctx = (ws2812_context_t *)context;
    ctx->state = SL_LED_CURRENT_STATE_OFF;
}

static void ws2812_led_toggle(void *context)
{
    ws2812_context_t *ctx = (ws2812_context_t *)context;
    if (ctx->state == SL_LED_CURRENT_STATE_ON) {
        ctx->state = SL_LED_CURRENT_STATE_OFF;
    } else {
        ctx->state = SL_LED_CURRENT_STATE_ON;
    }
}

static sl_led_state_t ws2812_led_get_state(void *context)
{
    ws2812_context_t *ctx = (ws2812_context_t *)context;
    return ctx->state;
}

static void ws2812_led_set_color(void *context, uint16_t red, uint16_t green, uint16_t blue)
{
    ws2812_context_t *ctx = (ws2812_context_t *)context;
    ctx->red = red;
    ctx->green = green;
    ctx->blue = blue;
}

static void ws2812_led_get_color(void *context, uint16_t *red, uint16_t *green, uint16_t *blue)
{
    ws2812_context_t *ctx = (ws2812_context_t *)context;
    *red = ctx->red;
    *green = ctx->green;
    *blue = ctx->blue;
}

void ws2812_led_driver_refresh(void)
{
    ws2812_context_t *ctx = &ws2812_context;

    if (ctx->state == SL_LED_CURRENT_STATE_ON) {
        uint8_t r = ctx->red >> 8;
        uint8_t g = ctx->green >> 8;
        uint8_t b = ctx->blue >> 8;
        ws2812_send_buffer(r, g, b);
    } else {
        ws2812_send_buffer(0, 0, 0);
    }
}

const sl_led_rgb_pwm_t sl_led_ws2812 = {
    .led_common = {
        .context = &ws2812_context,
        .init = ws2812_led_init,
        .turn_on = ws2812_led_turn_on,
        .turn_off = ws2812_led_turn_off,
        .toggle = ws2812_led_toggle,
        .get_state = ws2812_led_get_state
    },
    .set_rgb_color = ws2812_led_set_color,
    .get_rgb_color = ws2812_led_get_color
};

void ws2812_led_driver_init(void)
{
    precompute_ws2812_patterns();
    sl_led_init(&sl_led_ws2812.led_common);
}
