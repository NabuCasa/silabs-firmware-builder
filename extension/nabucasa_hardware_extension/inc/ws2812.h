/*
 * ws2812.h
 *
 * WS2812 addressable RGB LED driver
 * Author: qian
 */

#ifndef WS2812_H_
#define WS2812_H_

#include <stdint.h>

#include "ws2812_config.h"

typedef struct rgb_t{
  uint8_t G, R, B;
}rgb_t;

static const rgb_t off =    {   0,   0,   0 };
static const rgb_t red =    {   0, 255,   0 };
static const rgb_t yellow = { 255, 255,   0 };
static const rgb_t green =  { 255,   0,   0 };
static const rgb_t amber =  {  40, 255,   0 };
static const rgb_t white =  { 255, 255, 255 };

// Reduce the intensity a little to match the ZWA-2
static const rgb_t zwa2_white = { 75, 75, 75 };

rgb_t* get_color_buffer(void);
void set_color_buffer(const rgb_t *input_color);
void set_all_leds(const rgb_t *input_color);
void initWs2812(void);

#endif /* WS2812_H_ */
