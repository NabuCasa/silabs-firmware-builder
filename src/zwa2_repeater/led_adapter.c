#include "led_adapter.h"

#include "sl_led.h"
#include "ws2812.h"

void initWs2812(void)
{
  // The driver initializes itself through the driver_init event. It defaults
  // to a dim white color. Start black like the original driver did, and keep
  // the LED logically on. Blackness is controlled through the color alone.
  sl_led_set_rgb_color(&sl_led_ws2812, 0, 0, 0);
  sl_led_turn_on(&sl_led_ws2812.led_common);
  ws2812_led_driver_refresh();
}

void set_color_buffer(const rgb_t *color)
{
  // Shift into the driver's 16-bit range with a zero fraction, so its temporal
  // dithering stays inert and the 8-bit values are reproduced exactly.
  sl_led_set_rgb_color(&sl_led_ws2812,
                       (uint16_t)(color->R << 8),
                       (uint16_t)(color->G << 8),
                       (uint16_t)(color->B << 8));
  ws2812_led_driver_refresh();
}

void get_color_buffer(rgb_t *color)
{
  uint16_t red;
  uint16_t green;
  uint16_t blue;

  sl_led_get_rgb_color(&sl_led_ws2812, &red, &green, &blue);

  color->R = (uint8_t)(red >> 8);
  color->G = (uint8_t)(green >> 8);
  color->B = (uint8_t)(blue >> 8);
}
