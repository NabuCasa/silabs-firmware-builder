/*
 * ws2812.h
 *
 *  Created on: 2024年11月1日
 *      Author: qian
 */

#ifndef WS2812_H_
#define WS2812_H_

#include <stdint.h>
#include <stdlib.h>
#include "em_eusart.h"
#include "em_ldma.h"
#include "em_gpio.h"
#include "em_cmu.h"

#define EUS1MOSI_PORT   gpioPortC
#define EUS1MOSI_PIN    2
#define EUS1SCLK_PORT   gpioPortD
#define EUS1SCLK_PIN    2
#define WS2812_EN_PORT   gpioPortC
#define WS2812_EN_PIN    3

// LDMA channels for receive and transmit servicing
#define TX_LDMA_CHANNEL 1

// NEEDS TO BE USER DEFINED
#ifndef NUMBER_OF_LEDS
#define NUMBER_OF_LEDS         4
#endif

typedef struct rgb_t{
  uint8_t G, R, B;
}rgb_t;

static const rgb_t red = { 0x00, 0xFF, 0x00 };
static const rgb_t yellow = { 0xFF, 0xFF, 0x00 };
static const rgb_t white = { 0xE3, 0xFF, 0xB5 };

rgb_t* get_color_buffer();
void set_color_buffer(rgb_t *input_color);
void initWs2812(void);
// void color_test();

#endif /* WS2812_H_ */
