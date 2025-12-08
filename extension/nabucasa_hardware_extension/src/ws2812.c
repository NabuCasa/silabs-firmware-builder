/*
 * ws2812.c
 *
 * WS2812 addressable RGB LED driver using SPIDRV
 */

#include "ws2812.h"
#include <string.h>
#include "em_gpio.h"
#include "sl_spidrv_instances.h"

#define RESET_SIGNAL_BYTES       90
#define COLOR_BYTES_TOTAL        (WS2812_NUM_LEDS * 3)
#define SPI_BUFFER_SIZE_BYTES    (RESET_SIGNAL_BYTES + (COLOR_BYTES_TOTAL * 3))
#define WS2812_BIT_0             0b100
#define WS2812_BIT_1             0b110

static rgb_t rgb_color_buffer[WS2812_NUM_LEDS];
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

void initWs2812(void)
{
    GPIO_PinModeSet(WS2812_EN_PORT, WS2812_EN_PIN, gpioModePushPull, 1);
}

void set_color_buffer(const rgb_t *input_colors)
{
    if (input_colors != rgb_color_buffer) {
        memcpy(rgb_color_buffer, input_colors, sizeof(rgb_t) * WS2812_NUM_LEDS);
    }

    uint8_t *spi_ptr = &spi_tx_buffer[RESET_SIGNAL_BYTES];
    const uint8_t *color_ptr = (const uint8_t *)input_colors;

    for (int i = 0; i < COLOR_BYTES_TOTAL; i++) {
        uint32_t packed_pattern = byte_to_ws2812_pattern(*color_ptr++);

        *spi_ptr++ = (packed_pattern >> 16) & 0xFF;
        *spi_ptr++ = (packed_pattern >> 8)  & 0xFF;
        *spi_ptr++ = (packed_pattern)       & 0xFF;
    }

    SPIDRV_MTransmit(sl_spidrv_eusart_ws2812_handle, spi_tx_buffer, SPI_BUFFER_SIZE_BYTES, NULL);
}

void set_all_leds(const rgb_t *input_color)
{
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        rgb_color_buffer[i] = *input_color;
    }
 
    set_color_buffer(rgb_color_buffer);
}
