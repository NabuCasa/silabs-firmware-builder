/*
 * ws2812.c
 *
 * WS2812 addressable RGB LED driver using SPIDRV
 */

#include "ws2812.h"
#include <string.h>
#include "em_gpio.h"
#include "sl_spidrv_instances.h"

/*
 * T0H =   350ns +/- 150ns HIGH
 * T1H =   700ns +/- 150ns HIGH
 * T0L =   800ns +/- 150ns  LOW
 * T1L =   600ns +/- 150ns  LOW
 * RES > 50000ns            LOW
 *
 * 0 code = T0H + T0L
 * 1 code = T1H + T1L
 *
 * At 2400000 bits/second  ->  416.67 nanoseconds per bit
 *
 * Composition of 24bit data:
 *   G7 G6 G5 G4 G3 G2 G1 G0 R7 R6 R5 R4 R3 R2 R1 R0 B7 B6 B5 B4 B3 B2 B1 B0
 */
#define RESET_SIGNAL_BYTES     120 + 1
#define COLOR_BYTES_TOTAL      (WS2812_NUM_LEDS * 3)
#define SPI_BUFFER_SIZE_BYTES  (RESET_SIGNAL_BYTES + (COLOR_BYTES_TOTAL * 3))
#define WS2812_BIT_0           0b100
#define WS2812_BIT_1           0b110

static uint8_t spi_tx_buffer[SPI_BUFFER_SIZE_BYTES] = {0};

// Converts a byte (0-255) into the 24-bit SPI pattern (0b100.. or 0b110..)
static uint32_t byte_to_ws2812_pattern(uint8_t color_byte)
{
    uint32_t spi_pattern = 0;
    for (int i = 7; i >= 0; i--) {
        spi_pattern <<= 3;
        if (color_byte & (1 << i)) {
            spi_pattern |= WS2812_BIT_1;
        } else {
            spi_pattern |= WS2812_BIT_0;
        }
    }
    return spi_pattern;
}

// Internal context type
typedef struct {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    sl_led_state_t state;
} ws2812_context_t;

static ws2812_context_t ws2812_context = {
    .red = 19275,    // ~75/255 * 65535, dim white default
    .green = 19275,
    .blue = 19275,
    .state = SL_LED_CURRENT_STATE_OFF
};

static void ws2812_led_apply_color(ws2812_context_t *ctx)
{
    uint8_t r, g, b;

    if (ctx->state == SL_LED_CURRENT_STATE_ON) {
        r = ctx->red >> 8;
        g = ctx->green >> 8;
        b = ctx->blue >> 8;
    } else {
        r = 0;
        g = 0;
        b = 0;
    }

    // First `RESET_SIGNAL_BYTES` bytes are reserved for reset signal
    uint8_t *spi_ptr = &spi_tx_buffer[RESET_SIGNAL_BYTES];

    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        uint32_t packed_g = byte_to_ws2812_pattern(g);
        *spi_ptr++ = (packed_g >> 16) & 0xFF;
        *spi_ptr++ = (packed_g >>  8) & 0xFF;
        *spi_ptr++ = (packed_g >>  0) & 0xFF;

        uint32_t packed_r = byte_to_ws2812_pattern(r);
        *spi_ptr++ = (packed_r >> 16) & 0xFF;
        *spi_ptr++ = (packed_r >>  8) & 0xFF;
        *spi_ptr++ = (packed_r >>  0) & 0xFF;

        uint32_t packed_b = byte_to_ws2812_pattern(b);
        *spi_ptr++ = (packed_b >> 16) & 0xFF;
        *spi_ptr++ = (packed_b >>  8) & 0xFF;
        *spi_ptr++ = (packed_b >>  0) & 0xFF;
    }

    SPIDRV_MTransmit(sl_spidrv_eusart_ws2812_handle, spi_tx_buffer, SPI_BUFFER_SIZE_BYTES, NULL);
}

static sl_status_t ws2812_led_init(void *context)
{
    (void)context;
    GPIO_PinModeSet(WS2812_EN_PORT, WS2812_EN_PIN, gpioModePushPull, 1);
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
    ws2812_led_apply_color(&ws2812_context);
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
    sl_led_init(&sl_led_ws2812.led_common);
}
