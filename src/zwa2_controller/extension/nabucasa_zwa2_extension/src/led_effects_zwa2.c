/*
 * led_effects_zwa2.c
 *
 * LED System Behaviors for ZWA-2 Z-Wave Controller
 *
 * Adapted from NabuCasa/silabs-firmware-builder nabucasa_hardware_extension.
 * ZWA-2 differences vs ZBT-2:
 *   - Tilt monitoring is always active (not coupled to network state)
 *   - Tilt can be disabled via NABU_CASA_CONFIG_SET (NC_CFG_ENABLE_TILT_INDICATOR)
 *   - Network states: pulse white (searching), solid white (connected)
 *   - No device_has_stored_network_settings() extern (not applicable for Z-Wave NCP)
 */

#include "led_effects_zwa2.h"
#include "led_effects_config_zwa2.h"
#include "led_manager_zwa2.h"
#include "qma6100p.h"
#include "sl_sleeptimer.h"
#include "sl_i2cspm_instances.h"
#include <math.h>

#ifndef M_PI
#define M_PI  3.14159265358979323846f
#endif

// Internal state
static sl_sleeptimer_timer_handle_t tilt_monitor_timer;
static uint32_t monitor_ticks = 0;
static bool is_monitoring = false;
static bool was_tilted = false;
static bool tilt_enabled = true;

// Debounce state
static int16_t last_reading[3] = {0};
static int stable_count = 0;

// Tilt sample interval in timer ticks
#define TILT_SAMPLE_TICKS  (LED_EFFECTS_TILT_SAMPLE_MS / LED_EFFECTS_UPDATE_INTERVAL_MS)

static void tilt_monitor_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  monitor_ticks++;

  if (!tilt_enabled) {
    if (was_tilted) {
      led_manager_clear_pattern(LED_PRIORITY_TILT);
      was_tilted = false;
    }
    return;
  }

  if (monitor_ticks % TILT_SAMPLE_TICKS != 0) {
    return;
  }

  // Read raw accelerometer data
  int16_t reading[3];
  qma6100p_read_raw_xyz(sl_i2cspm_inst, reading);

  // Debounce: check if reading is stable compared to last
  int16_t dx = reading[0] - last_reading[0];
  int16_t dy = reading[1] - last_reading[1];
  int16_t dz = reading[2] - last_reading[2];
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  if (dz < 0) dz = -dz;

  last_reading[0] = reading[0];
  last_reading[1] = reading[1];
  last_reading[2] = reading[2];

  if (dx < LED_EFFECTS_TILT_STABLE_DELTA &&
      dy < LED_EFFECTS_TILT_STABLE_DELTA &&
      dz < LED_EFFECTS_TILT_STABLE_DELTA) {
    stable_count++;
  } else {
    stable_count = 0;
    return;
  }

  if (stable_count < LED_EFFECTS_TILT_STABLE_COUNT) {
    return;
  }

  // Calculate tilt angle from vertical (0° = vertical, 90° = horizontal)
  // asinf naturally treats both upright and upside-down as ~0°
  float mag = sqrtf((float)reading[0] * reading[0] +
                     (float)reading[1] * reading[1] +
                     (float)reading[2] * reading[2]);
  if (mag < 1.0f) {
    return;
  }

  float horizontal = sqrtf((float)reading[0] * reading[0] +
                            (float)reading[1] * reading[1]);
  float angle = asinf(horizontal / mag) * 180.0f / M_PI;

  bool is_tilted;
  if (was_tilted) {
    is_tilted = (angle > LED_EFFECTS_TILT_THRESHOLD_LOWER);
  } else {
    is_tilted = (angle > LED_EFFECTS_TILT_THRESHOLD_UPPER);
  }

  if (is_tilted && !was_tilted) {
    led_pattern_t tilt_pattern = {
        .mode = LED_MODE_BLINK,
        .color = LED_COLOR_COLD_WHITE,
        .period_ms = 500,
        .duration_ms = 0
    };
    led_manager_set_pattern(LED_PRIORITY_TILT, &tilt_pattern);
  } else if (!is_tilted && was_tilted) {
    led_manager_clear_pattern(LED_PRIORITY_TILT);
  }
  was_tilted = is_tilted;
}

static void start_tilt_monitor(void)
{
  if (is_monitoring) return;

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

void led_effects_init(void)
{
  led_manager_init();
  // ZWA-2: Tilt monitoring is always active (independent of network state)
  start_tilt_monitor();
}

void led_effects_set_searching(void)
{
  // Searching for host - Pulse white on BACKGROUND
  led_pattern_t search_pattern = {
      .mode = LED_MODE_PULSE,
      .color = LED_COLOR_COLD_WHITE,
      .period_ms = 2760,
      .duration_ms = 0,
      .brightness_min = 6554,  // ~10%
      .brightness_max = 65535,
  };
  led_manager_set_pattern(LED_PRIORITY_BACKGROUND, &search_pattern);
}

void led_effects_set_connected(void)
{
  // Host connected - Solid white on BACKGROUND
  led_manager_set_color(LED_PRIORITY_BACKGROUND, LED_COLOR_COLD_WHITE);
}

void led_effects_set_tilt_enabled(bool enabled)
{
  tilt_enabled = enabled;
  // If disabling and currently tilted, clear the blink immediately
  if (!enabled && was_tilted) {
    led_manager_clear_pattern(LED_PRIORITY_TILT);
    was_tilted = false;
  }
}
