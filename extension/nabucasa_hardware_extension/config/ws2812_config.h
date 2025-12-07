/***************************************************************************//**
 * @file
 * @brief Configuration header for WS2812 LED Driver
 ******************************************************************************/
#ifndef WS2812_CONFIG_H_
#define WS2812_CONFIG_H_

// <<< Use Configuration Wizard in Context Menu >>>

// <h>WS2812 Driver Configuration

// <o WS2812_EUSART_NUM> EUSART peripheral index
// <0=> EUSART0
// <1=> EUSART1
// <2=> EUSART2
// <i> Default: 1
#ifndef WS2812_EUSART_NUM
#warning "WS2812 EUSART peripheral not configured"
// #define WS2812_EUSART_NUM   1
#endif

// <o WS2812_LDMA_CHANNEL> LDMA channel for TX
// <i> Default: 1
#ifndef WS2812_LDMA_CHANNEL
#warning "WS2812 LDMA channel not configured"
// #define WS2812_LDMA_CHANNEL 1
#endif

// <o WS2812_NUM_LEDS> Number of addressable LEDs
// <i> Default: 4
#ifndef WS2812_NUM_LEDS
#warning "WS2812 LED count not configured"
// #define WS2812_NUM_LEDS     4
#endif

// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>

// <gpio> WS2812_MOSI
// $[GPIO_WS2812_MOSI]
#ifndef WS2812_MOSI_PORT
#warning "WS2812 MOSI port not configured"
// #define WS2812_MOSI_PORT    gpioPortC
// #define WS2812_MOSI_PIN     2
#endif
// [GPIO_WS2812_MOSI]$

// <gpio> WS2812_SCLK
// $[GPIO_WS2812_SCLK]
#ifndef WS2812_SCLK_PORT
#warning "WS2812 SCLK port not configured"
// #define WS2812_SCLK_PORT    gpioPortD
// #define WS2812_SCLK_PIN     2
#endif
// [GPIO_WS2812_SCLK]$

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
