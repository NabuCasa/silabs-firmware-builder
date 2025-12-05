/*
 * ws2812.h
 *
 * WS2812 addressable RGB LED driver
 */

#ifndef WS2812_H_
#define WS2812_H_

#include <stdint.h>
#include <stdlib.h>
#include "em_eusart.h"
#include "em_ldma.h"
#include "em_gpio.h"
#include "em_cmu.h"

#include "ws2812_config.h"
#include "ws2812_peripheral.h"

typedef struct rgb_t{
  uint8_t G, R, B;
}rgb_t;

static const rgb_t red = { 0x00, 0xFF, 0x00 };
static const rgb_t yellow = { 0xFF, 0xFF, 0x00 };
static const rgb_t white = { 0xFF, 0xFF, 0xFF };

// Reduce the intensity a little to match the ZWA-2
static const rgb_t zwa2_white = { 75, 75, 75 };

rgb_t* get_color_buffer(void);
void set_color_buffer(rgb_t *input_color);
void initWs2812(void);

#endif /* WS2812_H_ */
