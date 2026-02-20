/*
 * led_effects.c
 *
 * LED System Behaviors Implementation
 * High-level autonomous behaviors using the led_manager.
 */

#include "led_effects.h"
#include "led_effects_config.h"
#include "led_manager.h"
#include "qma6100p.h"
#include "sl_i2cspm_instances.h"
#include "sl_status.h"
#include "sl_sleeptimer.h"
#include <math.h>

#ifndef M_PI
#define M_PI  3.14159265358979323846f
#endif

// Internal state
static sl_sleeptimer_timer_handle_t tilt_monitor_timer;
static uint32_t monitor_ticks = 0;
static bool is_monitoring = false;
static bool was_tilted = false;

// Calculate tilt angle from accelerometer data
static float calculate_tilt_angle(void)
{
  float xyz[3];
  qma6100p_read_acc_xyz(sl_i2cspm_inst, xyz);

  float ax = xyz[0];
  float ay = xyz[1];
  float az = xyz[2];

  float total_mag = sqrtf(ax*ax + ay*ay + az*az);
  if (total_mag < 0.1f) {
    return 0.0f;
  }

  float horizontal_mag = sqrtf(ax*ax + ay*ay);
  float tilt_rad = asinf(horizontal_mag / total_mag);
  float tilt_deg = tilt_rad * 180.0f / M_PI;

  return tilt_deg;
}

static void tilt_monitor_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  monitor_ticks++;

  // Check tilt every ~100ms (25 * 4ms)
  if (monitor_ticks % 25 == 0) {
    float tilt_angle = calculate_tilt_angle();
    bool is_tilted;
    
    if (was_tilted) {
        is_tilted = (tilt_angle > (LED_EFFECTS_TILT_THRESHOLD_DEG - LED_EFFECTS_TILT_HYSTERESIS_DEG));
    } else {
        is_tilted = (tilt_angle > LED_EFFECTS_TILT_THRESHOLD_DEG);
    }

    if (is_tilted && !was_tilted) {
        // Device tilted - Set Fast Blink on CRITICAL layer
        led_pattern_t tilt_pattern = {
            .mode = LED_MODE_BLINK,
            .color = LED_COLOR_WHITE_DIM,
            .period_ms = 500,
            .duration_ms = 0
        };
        led_manager_set_pattern(LED_PRIORITY_CRITICAL, &tilt_pattern);
    } else if (!is_tilted && was_tilted) {
        // Tilted ended - Clear CRITICAL layer to reveal previous state
        led_manager_clear_pattern(LED_PRIORITY_CRITICAL);
    }
    was_tilted = is_tilted;
  }
}

void led_effects_init(void)
{
  led_manager_init();
}

void led_effects_set_network_state(bool network_formed)
{
    if (network_formed) {
        // Network formed - Background is OFF
        led_manager_clear_pattern(LED_PRIORITY_BACKGROUND);

        // Stop tilt monitoring to save power and stop blinking
        if (is_monitoring) {
             sl_sleeptimer_stop_timer(&tilt_monitor_timer);
             is_monitoring = false;
             
             // Ensure any active tilt blink is cleared
             led_manager_clear_pattern(LED_PRIORITY_CRITICAL);
        }
    } else {
        // Searching - Background is Pulse White
        led_pattern_t search_pattern = {
            .mode = LED_MODE_PULSE,
            .color = LED_COLOR_WHITE_DIM,
            .period_ms = 2760,
            .duration_ms = 0,
            .brightness_min = 6554,  // ~10%
            .brightness_max = 65535,
        };
        led_manager_set_pattern(LED_PRIORITY_BACKGROUND, &search_pattern);

        // Start tilt monitoring if not already running
        if (!is_monitoring) {
            monitor_ticks = 0;
            was_tilted = false;
            sl_sleeptimer_start_periodic_timer_ms(&tilt_monitor_timer,
                                        LED_EFFECTS_UPDATE_INTERVAL_MS,
                                        tilt_monitor_callback,
                                        NULL,
                                        0,
                                        0);
            is_monitoring = true;
        }
    }
}