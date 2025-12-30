/*
 * ws2812.c
 *
 * WS2812 addressable RGB LED driver using SPIDRV
 */

#pragma GCC optimize ("Ofast")

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
 * At 3200000 bits/second  ->  312.5  nanoseconds per bit
 *
 * Composition of 24bit data:
 *   G7 G6 G5 G4 G3 G2 G1 G0 R7 R6 R5 R4 R3 R2 R1 R0 B7 B6 B5 B4 B3 B2 B1 B0
 */
#define WS2812_BIT_0           0b1000
#define WS2812_BIT_1           0b1100

// 3 bits technically suffice to implement the protocol timing but this makes translating
// 3 x 8-bit RGB values into 9 bytes for SPI transfer require a bunch of bitwise ops.
// Instead, we bump the SPI clock speed and send 12 bytes via SPI, which align well.
#define WS2812_BITS            4

#define RESET_SIGNAL_BYTES     20
#define SPI_BUFFER_SIZE_BYTES  (RESET_SIGNAL_BYTES + (3 * (WS2812_NUM_LEDS * WS2812_BITS)))

SL_ALIGN(4) static uint8_t spi_tx_buffer[SPI_BUFFER_SIZE_BYTES] = {0};
SL_ALIGN(4) static uint32_t ws2812_lookup[256];


// Look up table for bit patterns, to avoid computing them at runtime
static void precompute_ws2812_patterns(void)
{
    for (int i = 0; i < 256; i++) {
        uint32_t spi_pattern = 0x00000000;

        for (int bit = 7; bit >= 0; bit--) {
            spi_pattern <<= WS2812_BITS;

            if (i & (1 << bit)) {
                spi_pattern |= WS2812_BIT_1;
            } else {
                spi_pattern |= WS2812_BIT_0;
            }
        }

        ws2812_lookup[i] = __builtin_bswap32(spi_pattern);
    }
}

// Temporal dithering: rapidly flicker between two 8-bit values, keeping track of the
// running error in `accum`. Returns the dithered 8-bit value.
static uint8_t dither_channel(uint16_t value, uint8_t *accum)
{
    uint8_t base = value >> 8;
    uint8_t frac = value & 0xFF;

    uint8_t prev = *accum;
    *accum += frac;

    // On overflow, output base + 1
    if (*accum < prev) {
        return base < 255 ? base + 1 : 255;
    }

    return base;
}

// Internal context type
typedef struct {
    uint16_t red;
    uint16_t green;
    uint16_t blue;

    uint8_t dither_accum_red;
    uint8_t dither_accum_green;
    uint8_t dither_accum_blue;

    sl_led_state_t state;
} ws2812_context_t;

static ws2812_context_t ws2812_context = {
    .red = 19275,    // ~75/255 * 65535, dim white default
    .green = 19275,
    .blue = 19275,
    .dither_accum_red = 0,
    .dither_accum_green = 0,
    .dither_accum_blue = 0,
    .state = SL_LED_CURRENT_STATE_OFF
};

static void ws2812_led_apply_color(ws2812_context_t *ctx)
{
    uint8_t r, g, b;

    if (ctx->state == SL_LED_CURRENT_STATE_ON) {
        r = dither_channel(ctx->red, &ctx->dither_accum_red);
        g = dither_channel(ctx->green, &ctx->dither_accum_green);
        b = dither_channel(ctx->blue, &ctx->dither_accum_blue);
    } else {
        r = 0;
        g = 0;
        b = 0;
    }

    // First `RESET_SIGNAL_BYTES` bytes are reserved for reset signal
    uint32_t *spi_ptr = (uint32_t*)&spi_tx_buffer[RESET_SIGNAL_BYTES];

    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        *spi_ptr++ = ws2812_lookup[g];
        *spi_ptr++ = ws2812_lookup[r];
        *spi_ptr++ = ws2812_lookup[b];
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
    precompute_ws2812_patterns();
    sl_led_init(&sl_led_ws2812.led_common);
}
