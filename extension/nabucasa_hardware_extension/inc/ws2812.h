/*
 * ws2812.h
 *
 * WS2812 addressable RGB LED driver
 * Author: qian
 */

#ifndef WS2812_H_
#define WS2812_H_

#include <stdint.h>

#include "sl_led.h"
#include "sl_simple_rgb_pwm_led.h"
#include "ws2812_config.h"

void ws2812_led_driver_init(void);

extern const sl_led_rgb_pwm_t sl_led_ws2812;

#endif /* WS2812_H_ */
