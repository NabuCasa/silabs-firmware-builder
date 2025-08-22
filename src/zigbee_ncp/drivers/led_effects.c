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
static led_state_t network_state = LED_STATE_NETWORK_NOT_FORMED; // Remember network state for tilt recovery
static sl_sleeptimer_timer_handle_t led_update_timer;
static uint32_t tick_counter = 0;

// Animation state
static uint16_t fade_step = 0;     // 0-499 for fade animation (500 steps total)
static bool blink_on = false;     // Current blink state for tilted mode
static uint32_t blink_counter = 0; // Counter for blink timing

// Animation parameters
static const uint32_t LED_UPDATE_INTERVAL_MS = 4;    // 4ms timer for all updates
static const uint16_t FADE_STEPS_TOTAL = 500;        // 500 steps * 4ms = 2s cycle
static const uint32_t BLINK_HALF_PERIOD_TICKS = 62;  // 62 * 4ms = 248ms ≈ 250ms
static const float TILT_THRESHOLD_DEGREES = 10.0f;   // Tilt threshold
static const float TILT_HYSTERESIS_DEGREES = 2.0f;   // Hysteresis to prevent flicker

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

// Update LED state based on tilt detection
static void update_tilt_state(void)
{
  static bool was_tilted = false;
  float tilt_angle = calculate_tilt_angle();
  
  // Apply hysteresis to prevent flickering
  bool is_tilted;
  if (was_tilted) {
    // Higher threshold to exit tilted state
    is_tilted = (tilt_angle > (TILT_THRESHOLD_DEGREES - TILT_HYSTERESIS_DEGREES));
  } else {
    // Lower threshold to enter tilted state
    is_tilted = (tilt_angle > TILT_THRESHOLD_DEGREES);
  }
  
  if (is_tilted && current_state != LED_STATE_TILTED) {
    // Enter tilted state
    current_state = LED_STATE_TILTED;
    blink_counter = 0;
    blink_on = true;
  } else if (!is_tilted && current_state == LED_STATE_TILTED) {
    // Exit tilted state - return to network state
    current_state = network_state;
    if (current_state == LED_STATE_NETWORK_NOT_FORMED) {
      fade_step = 0; // Reset fade animation
    }
  }
  
  was_tilted = is_tilted;
}

// Update fade animation for network not formed state
static void update_fade_animation(void)
{
  // Triangle wave: 0->255->0 over 500 steps
  uint8_t brightness;
  if (fade_step < FADE_STEPS_TOTAL / 2) {
    // First half: 0 to 255
    brightness = (uint8_t)((fade_step * 255) / (FADE_STEPS_TOTAL / 2));
  } else {
    // Second half: 255 to 0
    uint16_t reverse_step = FADE_STEPS_TOTAL - fade_step - 1;
    brightness = (uint8_t)((reverse_step * 255) / (FADE_STEPS_TOTAL / 2));
  }
  
  set_all_leds(brightness, brightness, brightness);
  
  fade_step++;
  if (fade_step >= FADE_STEPS_TOTAL) {
    fade_step = 0;
  }
}

// Update blink animation for tilted state
static void update_blink_animation(void)
{
  blink_counter++;
  
  if (blink_counter >= BLINK_HALF_PERIOD_TICKS) {
    blink_on = !blink_on;
    blink_counter = 0;
  }
  
  if (blink_on) {
    set_all_leds(255, 255, 255); // Full brightness
  } else {
    set_all_leds(0, 0, 0); // Off
  }
}

// Main LED update timer callback
static void led_update_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  
  tick_counter++;
  
  // Read accelerometer every 25 ticks (100ms)
  if (tick_counter % 25 == 0) {
    update_tilt_state();
  }
  
  // Update LED animation based on current state
  switch (current_state) {
    case LED_STATE_NETWORK_FORMED:
      set_all_leds(0, 0, 0); // LEDs off
      break;
      
    case LED_STATE_NETWORK_NOT_FORMED:
      update_fade_animation();
      break;
      
    case LED_STATE_TILTED:
      update_blink_animation();
      break;
  }
}

// Public API Implementation

void led_effects_init(void)
{
  // Initialize state machine
  current_state = LED_STATE_NETWORK_NOT_FORMED;
  network_state = LED_STATE_NETWORK_NOT_FORMED;
  tick_counter = 0;
  fade_step = 0;
  blink_on = false;
  blink_counter = 0;
  
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
  
  // Only update if network state actually changed
  if (network_state != new_state) {
    network_state = new_state;
    
    // If not currently tilted, update current state immediately
    if (current_state != LED_STATE_TILTED) {
      current_state = network_state;
      if (current_state == LED_STATE_NETWORK_NOT_FORMED) {
        fade_step = 0; // Reset fade animation
      }
    }
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