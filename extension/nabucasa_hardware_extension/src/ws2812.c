/*
 * ws2812.c
 *
 * WS2812 addressable RGB LED driver using SPIDRV
 */

#include "ws2812.h"

#include <string.h>

#include "em_gpio.h"
#include "sl_spidrv_instances.h"

// 3 color channels, 8 bits each
#define NUMBER_OF_COLOR_BITS     (WS2812_NUM_LEDS * 3 * 8)

// 3 SPI bits are required to make a full 1.25uS color bit
#define SPI_NUMBER_OF_BITS       (NUMBER_OF_COLOR_BITS * 3)

// How big the SPI buffer should be,
// the first 90 bytes should be empty to provide a >280uS reset signal
// Although the data sheet specifies that the reset signal must come
// after the data, it only works if we put it at the start of the buffer.
#define RESET_SIGNAL_BYTES       90
#define SPI_BUFFER_SIZE_BYTES    ((SPI_NUMBER_OF_BITS / 8) + RESET_SIGNAL_BYTES)

// Output buffer for SPI
static uint8_t spi_tx_buffer[SPI_BUFFER_SIZE_BYTES] = {0};
static rgb_t rgb_color_buffer[WS2812_NUM_LEDS];

// The WS2812 protocol interprets a signal that is 2/3 high 1/3 low as 1
// and 1/3 high 2/3 low as 0. This can be done by encoding each bit as 3 bits,
// where 110 is high and 100 is low.

// The bytes are interpreted LSB first, so the bit order is opposite from what the WS2812 protocol would suggest.
// Therefore, the full 3-byte sequence for a color byte is 0x10 x10x 10x1 0x10 x10x 10x1
// which is interpreted in reverse order:
#define FIRST_BYTE_DEFAULT  0b10010010
//                     bits   7  6  5
#define SECOND_BYTE_DEFAULT 0b01001001
//                     bits:   4  3
#define THIRD_BYTE_DEFAULT  0b00100100
//                     bits:   2  1  0

void initWs2812(void)
{
  GPIO_PinModeSet(WS2812_EN_PORT, WS2812_EN_PIN, gpioModePushPull, 1);
}

rgb_t* get_color_buffer(void)
{
  return rgb_color_buffer;
}

void set_color_buffer(rgb_t *input_colors)
{
  // Remember the current color for later querying
  memcpy(rgb_color_buffer, input_colors, sizeof(rgb_t) * WS2812_NUM_LEDS);

  const uint8_t *input_color_byte = (uint8_t *) input_colors;

  // See above for a more detailed description of the protocol and bit order
  uint32_t buffer_index = 0;
  while (buffer_index < SPI_BUFFER_SIZE_BYTES - RESET_SIGNAL_BYTES) {
    // FIRST BYTE
    // Isolate bit 7 and shift to position 6
    uint8_t bit_7 = (uint8_t)((*input_color_byte & 0x80) >> 1);
    // Isolate bit 6 and shift to position 3
    uint8_t bit_6 = (uint8_t)((*input_color_byte & 0x40) >> 3);
    // Isolate bit 5 and shift to position 0
    uint8_t bit_5 = (uint8_t)((*input_color_byte & 0x20) >> 5);
    // Load byte into the TX buffer
    spi_tx_buffer[buffer_index + RESET_SIGNAL_BYTES] = bit_7 | bit_6 | bit_5 | FIRST_BYTE_DEFAULT;
    buffer_index++;

    // SECOND BYTE
    // Isolate bit 4 and shift to position 5
    uint8_t bit_4 = (uint8_t)((*input_color_byte & 0x10) << 1);
    // Isolate bit 3 and shift to position 2
    uint8_t bit_3 = (uint8_t)((*input_color_byte & 0x08) >> 1);
    // Load byte into the TX buffer
    spi_tx_buffer[buffer_index + RESET_SIGNAL_BYTES] = bit_4 | bit_3 | SECOND_BYTE_DEFAULT;
    buffer_index++;

    // THIRD BYTE
    // Isolate bit 2 and shift to position 7
    uint8_t bit_2 = (uint8_t)((*input_color_byte & 0x04) << 5);
    // Isolate bit 1 and shift to position 4
    uint8_t bit_1 = (uint8_t)((*input_color_byte & 0x02) << 3);
    // Isolate bit 0 and shift to position 1
    uint8_t bit_0 = (uint8_t)((*input_color_byte & 0x01) << 1);
    // Load byte into the TX buffer
    spi_tx_buffer[buffer_index + RESET_SIGNAL_BYTES] = bit_2 | bit_1 | bit_0 | THIRD_BYTE_DEFAULT;
    buffer_index++;

    input_color_byte++;
  }

  SPIDRV_MTransmit(sl_spidrv_eusart_ws2812_handle, spi_tx_buffer, SPI_BUFFER_SIZE_BYTES, NULL);
}

void set_all_leds(const rgb_t *input_color)
{
  rgb_t colors[WS2812_NUM_LEDS];
  for (int i = 0; i < WS2812_NUM_LEDS; i++) {
    colors[i] = *input_color;
  }
  set_color_buffer(colors);
}
