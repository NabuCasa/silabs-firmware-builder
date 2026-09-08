/**
 * Adapter between the repeater's LED code and the shared WS2812 driver.
 *
 * The original firmware used a driver with a per-LED color buffer. The shared
 * driver holds a single color for all LEDs, which is all the repeater needs.
 */

#ifndef LED_ADAPTER_H_
#define LED_ADAPTER_H_

#include <stdint.h>
#include "ws2812_config.h"

#define NUMBER_OF_LEDS WS2812_NUM_LEDS

typedef struct rgb_t {
  uint8_t G, R, B;
} rgb_t;

void initWs2812(void);

// Apply the color to all LEDs
void set_color_buffer(const rgb_t *color);

// Read back the current color
void get_color_buffer(rgb_t *color);

#endif /* LED_ADAPTER_H_ */
