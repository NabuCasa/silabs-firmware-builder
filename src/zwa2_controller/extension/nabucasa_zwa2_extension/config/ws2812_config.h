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
#define WS2812_NUM_LEDS     4
#endif

// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>

// <gpio> WS2812_EN
// $[GPIO_WS2812_EN]
#ifndef WS2812_EN_PORT
#define WS2812_EN_PORT      gpioPortC
#define WS2812_EN_PIN       3
#endif
// [GPIO_WS2812_EN]$

// <<< sl:end pin_tool >>>

// EUSART SPI pin assignments for WS2812 data output
// TX = MOSI (data), SCLK = clock (active but unused by WS2812 protocol)
#ifndef WS2812_SPI_PERIPHERAL
#define WS2812_SPI_PERIPHERAL       EUSART1
#define WS2812_SPI_PERIPHERAL_NO    1
#endif

#ifndef WS2812_SPI_TX_PORT
#define WS2812_SPI_TX_PORT          gpioPortC
#define WS2812_SPI_TX_PIN           2
#endif

#ifndef WS2812_SPI_SCLK_PORT
#define WS2812_SPI_SCLK_PORT        gpioPortC
#define WS2812_SPI_SCLK_PIN         1
#endif

#endif // WS2812_CONFIG_H_
