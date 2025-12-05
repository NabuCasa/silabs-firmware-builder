/*
 * ws2812_peripheral.h
 *
 * EUSART peripheral abstraction macros for WS2812 driver
 */

#ifndef WS2812_PERIPHERAL_H_
#define WS2812_PERIPHERAL_H_

#include "ws2812_config.h"

// Concatenation helpers
#define WS2812_CONCAT2(a, b) a ## b
#define WS2812_CONCAT(a, b) WS2812_CONCAT2(a, b)

// EUSART peripheral and clock selection based on WS2812_EUSART_NUM
#define WS2812_EUSART       WS2812_CONCAT(EUSART, WS2812_EUSART_NUM)
#define WS2812_EUSART_CLOCK WS2812_CONCAT(cmuClock_EUSART, WS2812_EUSART_NUM)

// LDMA peripheral signal (requires specific handling per EUSART)
#if WS2812_EUSART_NUM == 0
  #define WS2812_LDMA_SIGNAL ldmaPeripheralSignal_EUSART0_TXFL
#elif WS2812_EUSART_NUM == 1
  #define WS2812_LDMA_SIGNAL ldmaPeripheralSignal_EUSART1_TXFL
#elif WS2812_EUSART_NUM == 2
  #define WS2812_LDMA_SIGNAL ldmaPeripheralSignal_EUSART2_TXFL
#else
  #error "Invalid WS2812_EUSART_NUM"
#endif

#endif /* WS2812_PERIPHERAL_H_ */
