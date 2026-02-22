/***************************************************************************//**
 * @file
 * @brief Configuration header for WS2812 LED Driver
 ******************************************************************************/
#ifndef WS2812_CONFIG_H_
#define WS2812_CONFIG_H_

// <<< Use Configuration Wizard in Context Menu >>>

// <h>WS2812 Driver Configuration

// <o WS2812_NUM_LEDS> Number of addressable LEDs
// <i> Default: 4
#ifndef WS2812_NUM_LEDS
#warning "WS2812 LED count not configured"
// #define WS2812_NUM_LEDS     4
#endif

// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>

// <gpio> WS2812_EN
// $[GPIO_WS2812_EN]
#ifndef WS2812_EN_PORT
#warning "WS2812 enable port not configured"
// #define WS2812_EN_PORT      gpioPortC
// #define WS2812_EN_PIN       3
#endif
// [GPIO_WS2812_EN]$

// <<< sl:end pin_tool >>>

#endif // WS2812_CONFIG_H_
