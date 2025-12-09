/*
 * led_effects.c
 *
 * LED State Machine Implementation
 * Simple state-based LED control for adapter status indication
 */

#include "led_effects.h"
#include "led_effects_config.h"
#include "qma6100p.h"
#include "ws2812.h"
#include "sl_i2cspm_instances.h"
#include "sl_status.h"
#include "sl_simple_rgb_pwm_led.h"
#include <string.h>
#include <math.h>

#define COLOR_OFF_R      (0 * 257)
#define COLOR_OFF_G      (0 * 257)
#define COLOR_OFF_B      (0 * 257)
#define COLOR_WHITE_R    (75 * 257)
#define COLOR_WHITE_G    (75 * 257)
#define COLOR_WHITE_B    (75 * 257)

// LED State Machine - Internal State
static led_state_t current_state = LED_STATE_NETWORK_NOT_FORMED;
static bool network_formed_state = false;
static sl_sleeptimer_timer_handle_t led_update_timer;
static uint32_t tick_counter = 0;

// Calculate tilt angle from accelerometer data
static float calculate_tilt_angle(void)
{
  float xyz[3];
  qma6100p_read_acc_xyz(sl_i2cspm_inst, xyz);

  // Calculate tilt angle using accelerometer data
  // For a device at rest, gravity provides 1g acceleration
  // Tilt angle = arcsin(sqrt(ax^2 + ay^2) / |total_acceleration|)
  float ax = xyz[0];
  float ay = xyz[1];
  float az = xyz[2];

  float total_mag = sqrtf(ax*ax + ay*ay + az*az);
  if (total_mag < 0.1f) {
    return 0.0f; // Avoid division by zero
  }

  float horizontal_mag = sqrtf(ax*ax + ay*ay);
  float tilt_rad = asinf(horizontal_mag / total_mag);
  float tilt_deg = tilt_rad * 180.0f / M_PI;

  return tilt_deg;
}

// Main LED update timer callback
static void led_update_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;

  tick_counter++;

  // Update state based on tilt
  static bool was_tilted = false;
  if (tick_counter % 25 == 0) { // Check every 100ms
    float tilt_angle = calculate_tilt_angle();
    bool is_tilted;
    if (was_tilted) {
        is_tilted = (tilt_angle > (LED_EFFECTS_TILT_THRESHOLD_DEG - LED_EFFECTS_TILT_HYSTERESIS_DEG));
    } else {
        is_tilted = (tilt_angle > LED_EFFECTS_TILT_THRESHOLD_DEG);
    }

    if (is_tilted && current_state != LED_STATE_TILTED) {
        current_state = LED_STATE_TILTED;
    } else if (!is_tilted && current_state == LED_STATE_TILTED) {
        // Return to the network-dependent state
        current_state = network_formed_state ? LED_STATE_NETWORK_FORMED : LED_STATE_NETWORK_NOT_FORMED;
    }
    was_tilted = is_tilted;
  }

  // Update LED animation based on current state
  switch (current_state) {
    case LED_STATE_NETWORK_FORMED:
      sl_led_turn_off(&sl_led_ws2812.led_common);
      break;

    case LED_STATE_NETWORK_NOT_FORMED: {
      // Gentle pulse (triangle wave over a ~2.76-second cycle)
      // Brightness ranges from ~10% to 100% of base white
      uint32_t step = tick_counter % 690;
      uint16_t brightness;
      if (step < 345) {
        brightness = 6553 + (step * (65535 - 6553)) / 345;  // 10% to 100%
      } else {
        brightness = 65535 - ((step - 345) * (65535 - 6553)) / 345;
      }
      // Scale base white by brightness
      uint16_t color = (uint32_t)COLOR_WHITE_R * brightness / 65535;
      sl_led_set_rgb_color(&sl_led_ws2812, color, color, color);
      sl_led_turn_on(&sl_led_ws2812.led_common);
      break;
    }

    case LED_STATE_TILTED: {
      // Fast blink (248ms half-period)
      if ((tick_counter / 62) % 2) {
        sl_led_set_rgb_color(&sl_led_ws2812, COLOR_WHITE_R, COLOR_WHITE_G, COLOR_WHITE_B);
        sl_led_turn_on(&sl_led_ws2812.led_common);
      } else {
        sl_led_turn_off(&sl_led_ws2812.led_common);
      }
      break;
    }
  }
}

// Public API Implementation

void led_effects_init(void)
{
  // Initialize state machine
  current_state = LED_STATE_NETWORK_NOT_FORMED;
  tick_counter = 0;
}

void led_effects_set_network_state(bool network_formed)
{
    network_formed_state = network_formed;
    bool is_running;

    // If the network is formed, stop all effects and turn off LEDs
    if (network_formed) {
        if (sl_sleeptimer_is_timer_running(&led_update_timer, &is_running) == SL_STATUS_OK && is_running) {
            sl_sleeptimer_stop_timer(&led_update_timer);
        }
        sl_led_turn_off(&sl_led_ws2812.led_common);
        current_state = LED_STATE_NETWORK_FORMED;
        return;
    }

    // If the network is not formed, start the timer if it's not running
    if (sl_sleeptimer_is_timer_running(&led_update_timer, &is_running) != SL_STATUS_OK || !is_running) {
        sl_sleeptimer_start_periodic_timer_ms(&led_update_timer,
                                              LED_EFFECTS_UPDATE_INTERVAL_MS,
                                              led_update_callback,
                                              NULL,
                                              0,
                                              0);
    }

    // Set the state to not formed if we are not tilted
    if (current_state != LED_STATE_TILTED) {
        current_state = LED_STATE_NETWORK_NOT_FORMED;
    }
}

void led_effects_stop_all(void)
{
  bool is_running;
  if (sl_sleeptimer_is_timer_running(&led_update_timer, &is_running) == SL_STATUS_OK && is_running) {
    sl_sleeptimer_stop_timer(&led_update_timer);
  }
  sl_led_turn_off(&sl_led_ws2812.led_common);
}

led_state_t led_effects_get_state(void)
{
  return current_state;
}
