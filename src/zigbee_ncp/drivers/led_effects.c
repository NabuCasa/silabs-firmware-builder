/*
 * led_effects.c
 *
 * LED State Machine Implementation
 * Simple state-based LED control for adapter status indication
 */

#include "led_effects.h"
#include "qma6100p.h"
#include <string.h>
#include <math.h>

// LED State Machine - Internal State
static led_state_t current_state = LED_STATE_NETWORK_NOT_FORMED;
static sl_sleeptimer_timer_handle_t led_update_timer;
static uint32_t tick_counter = 0;

// Animation parameters
static const uint32_t LED_UPDATE_INTERVAL_MS = 4;    // 4ms timer for all updates
static const float TILT_THRESHOLD_DEGREES = 16.0f;   // Tilt threshold
static const float TILT_HYSTERESIS_DEGREES = 4.0f;   // Hysteresis to prevent flicker
static const float M_PI = 3.14159265358979323846f;

// Calculate tilt angle from accelerometer data
static float calculate_tilt_angle(void)
{
  float xyz[3];
  qma6100p_read_acc_xyz(xyz);

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

// Apply color to all LEDs
static void set_all_leds(uint8_t r, uint8_t g, uint8_t b)
{
  rgb_t colors[4];
  for (int i = 0; i < 4; i++) {
    colors[i].R = r;
    colors[i].G = g;
    colors[i].B = b;
  }
  set_color_buffer(colors);
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
        is_tilted = (tilt_angle > (TILT_THRESHOLD_DEGREES - TILT_HYSTERESIS_DEGREES));
    } else {
        is_tilted = (tilt_angle > TILT_THRESHOLD_DEGREES);
    }

    if (is_tilted && current_state != LED_STATE_TILTED) {
        current_state = LED_STATE_TILTED;
    } else if (!is_tilted && current_state == LED_STATE_TILTED) {
        // Return to the network-dependent state
        led_effects_set_network_state(led_effects_get_state() == LED_STATE_NETWORK_FORMED);
    }
    was_tilted = is_tilted;
  }

  // Update LED animation based on current state
  switch (current_state) {
    case LED_STATE_NETWORK_FORMED:
      set_all_leds(0, 0, 0); // Off
      break;

    case LED_STATE_NETWORK_NOT_FORMED: {
      // Gentle pulse (triangle wave over a 2-second cycle)
      uint32_t step = tick_counter % 500;
      uint8_t brightness;
      if (step < 250) {
        // Fade in
        brightness = (step * 255) / 250;
      } else {
        // Fade out
        brightness = 255 - ((step - 250) * 255) / 250;
      }
      set_all_leds(brightness, brightness, brightness);
      break;
    }

    case LED_STATE_TILTED: {
      // Fast pulse (blink with a 248ms half-period)
      if ((tick_counter / 62) % 2) {
        set_all_leds(255, 255, 255);
      } else {
        set_all_leds(0, 0, 0);
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

  // Start the main LED update timer (4ms periodic)
  sl_sleeptimer_start_periodic_timer_ms(&led_update_timer,
                                        LED_UPDATE_INTERVAL_MS,
                                        led_update_callback,
                                        NULL,
                                        0,
                                        0);
}

void led_effects_set_network_state(bool network_formed)
{
    led_state_t new_state = network_formed ? LED_STATE_NETWORK_FORMED : LED_STATE_NETWORK_NOT_FORMED;
    // Only update if not tilted
    if (current_state != LED_STATE_TILTED) {
        current_state = new_state;
    }
}

void led_effects_stop_all(void)
{
  sl_sleeptimer_stop_timer(&led_update_timer);
  set_all_leds(0, 0, 0); // Turn off all LEDs
}

led_state_t led_effects_get_state(void)
{
  return current_state;
}