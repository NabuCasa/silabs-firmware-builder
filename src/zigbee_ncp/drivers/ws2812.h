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

static const rgb_t off =    {.R = 0,   .G = 0,   .B = 0};
static const rgb_t red =    {.R = 255, .G = 0,   .B = 0};
static const rgb_t yellow = {.R = 255, .G = 255, .B = 0};
static const rgb_t amber =  {.R = 255, .G = 191, .B = 0};
static const rgb_t white =  {.R = 255, .G = 255, .B = 255};

// Reduce the intensity a little to match the ZWA-2
static const rgb_t zwa2_white = {.R = 75, .G = 75, .B = 75};

rgb_t* get_color_buffer();
void set_color_buffer(rgb_t *input_color);
void set_all_leds(const rgb_t *input_color);
void initWs2812(void);
// void color_test();

#endif /* WS2812_H_ */
