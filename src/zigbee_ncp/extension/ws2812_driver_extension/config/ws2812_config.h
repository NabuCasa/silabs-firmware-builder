/*
 * ws2812_config.h
 *
 * Configuration for WS2812 LED driver
 */

#ifndef WS2812_CONFIG_H_
#define WS2812_CONFIG_H_

// EUSART MOSI pin (data out to LEDs)
#define WS2812_MOSI_PORT    gpioPortC
#define WS2812_MOSI_PIN     2

// EUSART SCLK pin (directly driven, unused by WS2812)
#define WS2812_SCLK_PORT    gpioPortD
#define WS2812_SCLK_PIN     2

// Enable pin for LED power
#define WS2812_EN_PORT      gpioPortC
#define WS2812_EN_PIN       3

// EUSART peripheral index (0, 1, or 2)
#define WS2812_EUSART_NUM   1

// LDMA channel for TX
#define WS2812_LDMA_CHANNEL 1

// Number of addressable LEDs
#define WS2812_NUM_LEDS     4

#endif /* WS2812_CONFIG_H_ */
