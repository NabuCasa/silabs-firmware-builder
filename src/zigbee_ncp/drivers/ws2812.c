/*
 * ws2812.c
 *
 *  Created on: 2024年11月1日
 *      Author: qian
 */


// BSP for board controller pin macros
#include "ws2812.h"
#include <string.h>

// LDMA descriptor and transfer configuration structures for TX channel
LDMA_Descriptor_t ldmaTXDescriptor;
LDMA_TransferCfg_t ldmaTXConfig;

// Frequency for the protocol in Hz, 800 kHz gives a 1.25uS duty cycle
#define PROTOCOL_FREQUENCY       800000

// 3 USART bits are required to make a full 1.25uS color bit
// USART frequency should therefore be 3x the protocol frequency
#define REQUIRED_USART_FREQUENCY (PROTOCOL_FREQUENCY * 3)

// 3 color channels, 8 bits each
#define NUMBER_OF_COLOR_BITS     (NUMBER_OF_LEDS * 3 * 8)

// 3 USART bits are required to make a full 1.25uS color bit,
// each USART bit is 416nS
#define USART_NUMBER_OF_BITS     (NUMBER_OF_COLOR_BITS * 3)

// How big the USART buffer should be,
// the first 90 bytes should be empty to provide a >280uS reset signal
// Although the data sheet specifies that the reset signal must come
// after the data, it only works if we put it at the start of the buffer.
#define RESET_SIGNAL_BYTES       90
#define USART_BUFFER_SIZE_BYTES  ((USART_NUMBER_OF_BITS / 8) + RESET_SIGNAL_BYTES)

// Output buffer for USART
static uint8_t USART_tx_buffer[USART_BUFFER_SIZE_BYTES] = {0};
static rgb_t rgb_color_buffer;

// The WS2812 protocol interprets a signal that is 2/3 high 1/3 low as 1
// and 1/3 high 2/3 low as 0. This can be done by encoding each bit as 3 bits,
// where 110 is high and 100 is low.

// The bytes are interpreted LSB first, so the bit order is opposite from what the WS2812 protocol would suggest.
// Therefore, the full 3-byte sequence for a color byte is 0x10 x10x 10x1 0x10 x10x 10x1
// which is interpreted in reverse order:
#define FIRST_BYTE_DEFAULT  0b10010010;
//                     bits:   7  6  5
#define SECOND_BYTE_DEFAULT 0b01001001;
//                     bits:    4  3
#define THIRD_BYTE_DEFAULT  0b00100100;
//                     bits:  2  1  0

#define LED_INTENSITY 100 // defines the intensity of the LEDs for the test
/**************************************************************************//**
 * @brief
 *    CMU initialization
 *****************************************************************************/
static void initCMU(void)
{
  // Enable clock to GPIO and EUSART1
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_EUSART1, true);
}

/**************************************************************************//**
 * @brief
 *    GPIO initialization
 *****************************************************************************/
static void initGPIO(void)
{
  // Configure MOSI (TX) pin as an output
  GPIO_PinModeSet(EUS1MOSI_PORT, EUS1MOSI_PIN, gpioModePushPull, 1);

  // Configure SCLK pin as an output low (CPOL = 0)
  GPIO_PinModeSet(EUS1SCLK_PORT, EUS1SCLK_PIN, gpioModePushPull, 1);

  GPIO_PinModeSet(WS2812_EN_PORT, WS2812_EN_PIN, gpioModePushPull, 1);
}

/**************************************************************************//**
 * @brief
 *    EUSART1 initialization
 *****************************************************************************/
static void initEUSART1(void)
{
  // SPI advanced configuration (part of the initializer)
  EUSART_SpiAdvancedInit_TypeDef adv = EUSART_SPI_ADVANCED_INIT_DEFAULT;

  adv.msbFirst = true;        // SPI standard MSB first
//  adv.invertIO = eusartInvertTxEnable;
//  adv.autoInterFrameTime = 7; // 7 bit times of delay between frames
//                              // to accommodate non-DMA secondaries

  // Default asynchronous initializer (main/master mode and 8-bit data)
  EUSART_SpiInit_TypeDef init = EUSART_SPI_MASTER_INIT_DEFAULT_HF;

  init.bitRate = REQUIRED_USART_FREQUENCY;
  init.advancedSettings = &adv;   // Advanced settings structure

  /*
   * Route EUSART1 MOSI, MISO, and SCLK to the specified pins.  CS is
   * not controlled by EUSART1 so there is no write to the corresponding
   * EUSARTROUTE register to do this.
   */
  GPIO->EUSARTROUTE[1].TXROUTE = (EUS1MOSI_PORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
      | (EUS1MOSI_PIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[1].SCLKROUTE = (EUS1SCLK_PORT << _GPIO_EUSART_SCLKROUTE_PORT_SHIFT)
      | (EUS1SCLK_PIN << _GPIO_EUSART_SCLKROUTE_PIN_SHIFT);

  // Enable EUSART interface pins
  GPIO->EUSARTROUTE[1].ROUTEEN = GPIO_EUSART_ROUTEEN_TXPEN |    // MOSI
                                 GPIO_EUSART_ROUTEEN_SCLKPEN;

  // Configure and enable EUSART1
  EUSART_SpiInit(EUSART1, &init);
}

/**************************************************************************//**
 * @brief
 *    LDMA initialization
 *****************************************************************************/
void initLDMA(void)
{
  // First, initialize the LDMA unit itself
  LDMA_Init_t ldmaInit = LDMA_INIT_DEFAULT;
  LDMA_Init(&ldmaInit);

  // Source is USART_tx_buffer, destination is EUSART1_TXDATA, and length if BUFLEN
  ldmaTXDescriptor = (LDMA_Descriptor_t)LDMA_DESCRIPTOR_SINGLE_M2P_BYTE(USART_tx_buffer, &(EUSART1->TXDATA), USART_BUFFER_SIZE_BYTES);

  // Transfer a byte on free space in the EUSART FIFO
  ldmaTXConfig = (LDMA_TransferCfg_t)LDMA_TRANSFER_CFG_PERIPHERAL(ldmaPeripheralSignal_EUSART1_TXFL);
}

void initWs2812(void)
{
  // Initialize GPIO and USART0
  initCMU();
  initGPIO();
  initEUSART1();
  initLDMA();
}

rgb_t get_color_buffer()
{
  return rgb_color_buffer;
}

void set_color_buffer(rgb_t input_color)
{
  // Remember the current color for later querying
  rgb_color_buffer = input_color;

  // Make it easier to access the color bytes using a pointer
  rgb_t input_colors[NUMBER_OF_LEDS] = {
    input_color,
    input_color,
    input_color,
    input_color
  };
  const uint8_t *input_color_byte = (uint8_t *) input_colors;

  // See above for a more detailed description of the protocol and bit order
  uint32_t usart_buffer_index = 0;
  while (usart_buffer_index < USART_BUFFER_SIZE_BYTES - RESET_SIGNAL_BYTES) {
    // FIRST BYTE
    // Isolate bit 7 and shift to position 6
    uint8_t bit_7 = (uint8_t)((*input_color_byte & 0x80) >> 1);
    // Isolate bit 6 and shift to position 3
    uint8_t bit_6 = (uint8_t)((*input_color_byte & 0x40) >> 3);
    // Isolate bit 5 and shift to position 0
    uint8_t bit_5 = (uint8_t)((*input_color_byte & 0x20) >> 5);
    // Load byte into the TX buffer
    USART_tx_buffer[usart_buffer_index + RESET_SIGNAL_BYTES] = bit_7 | bit_6 | bit_5 | FIRST_BYTE_DEFAULT;
    usart_buffer_index++;  // Increment USART_tx_buffer pointer

    // SECOND BYTE
    // Isolate bit 4 and shift to position 5
    uint8_t bit_4 = (uint8_t)((*input_color_byte & 0x10) << 1);
    // Isolate bit 3 and shift to position 2
    uint8_t bit_3 = (uint8_t)((*input_color_byte & 0x08) >> 1);
    // Load byte into the TX buffer
    USART_tx_buffer[usart_buffer_index + RESET_SIGNAL_BYTES] = bit_4 | bit_3 | SECOND_BYTE_DEFAULT;
    usart_buffer_index++; // Increment USART_tx_buffer pointer

    // THIRD BYTE
    // Isolate bit 2 and shift to position 7
    uint8_t bit_2 = (uint8_t)((*input_color_byte & 0x04) << 5);
    // Isolate bit 1 and shift to position 4
    uint8_t bit_1 = (uint8_t)((*input_color_byte & 0x02) << 3);
    // Isolate bit 0 and shift to position 1
    uint8_t bit_0 = (uint8_t)((*input_color_byte & 0x01) << 1);
    // Load byte into the TX buffer
    USART_tx_buffer[usart_buffer_index + RESET_SIGNAL_BYTES] = bit_2 | bit_1 | bit_0 | THIRD_BYTE_DEFAULT;
    usart_buffer_index++; // Increment USART_tx_buffer pointer
    
    input_color_byte++; // move to the next color byte
  }

  LDMA_StartTransfer(TX_LDMA_CHANNEL, &ldmaTXConfig, &ldmaTXDescriptor);
}
