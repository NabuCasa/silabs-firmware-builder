/*
 * led_effects.c
 *
 * LED Color Transition and Effects System Implementation
 */

#include "led_effects.h"
#include <string.h>

// LED Color Transition System - Internal State
static rgb_t current_color = {0, 0, 0};              // Current LED color
static rgb_t target_color = {0, 0, 0};               // Target LED color  
static uint32_t transition_end_time = 0;             // When transition should complete
static sl_sleeptimer_timer_handle_t color_transition_timer;  // Fast timer for smooth transitions
static sl_sleeptimer_timer_handle_t pulse_effect_timer;      // Timer for pulse effect
static bool pulse_active = false;                    // Track if pulse effect is running

// Animation parameters
static const uint32_t transition_update_interval_ms = 30;    // Update every 30ms for smooth transitions
static const uint32_t pulse_toggle_interval_ms = 2 * (0xFF - 25) * 4;  // Pulse timing
static const uint32_t default_transition_duration_ms = pulse_toggle_interval_ms; // Default transition time

// Pulse colors
static const rgb_t pulse_color_min = {25, 25, 25};   // Minimum pulse color
static const rgb_t pulse_color_max = {75, 75, 75};   // Maximum pulse color

// Linear interpolation between two uint8_t values
static uint8_t lerp_uint8(uint8_t a, uint8_t b, float t)
{
  if (t <= 0.0f) return a;
  if (t >= 1.0f) return b;
  return (uint8_t)(a + t * (b - a));
}

// Check if two colors are equal
static bool colors_equal(const rgb_t *a, const rgb_t *b)
{
  return (a->R == b->R && a->G == b->G && a->B == b->B);
}

// Apply current color to all LEDs
static void apply_current_color(void)
{
  rgb_t colors[4];
  for (int i = 0; i < 4; i++) {
    colors[i] = current_color;
  }
  set_color_buffer(colors);
}

// Color transition timer callback - handles smooth color transitions
static void color_transition_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  
  // Check if we've reached the target
  if (colors_equal(&current_color, &target_color)) {
    // Stop the transition timer - we're at the target
    sl_sleeptimer_stop_timer(&color_transition_timer);
    return;
  }
  
  // Calculate transition progress
  uint32_t current_time = sl_sleeptimer_get_tick_count();
  
  if (current_time >= transition_end_time) {
    // Transition complete - snap to target
    current_color = target_color;
  } else {
    // Calculate how far through the transition we are (0.0 to 1.0)
    uint32_t transition_start_time = transition_end_time - sl_sleeptimer_ms_to_tick(default_transition_duration_ms);
    uint32_t elapsed_ticks = current_time - transition_start_time;
    float progress = (float)sl_sleeptimer_tick_to_ms(elapsed_ticks) / (float)default_transition_duration_ms;
    
    // Clamp progress to [0.0, 1.0]
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    
    // Interpolate current color towards target
    current_color.R = lerp_uint8(current_color.R, target_color.R, progress);
    current_color.G = lerp_uint8(current_color.G, target_color.G, progress);
    current_color.B = lerp_uint8(current_color.B, target_color.B, progress);
  }
  
  // Apply the color
  apply_current_color();
}

// Pulse effect timer callback - toggles between pulse colors
static void pulse_effect_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  
  // Toggle between min and max pulse colors
  if (colors_equal(&target_color, &pulse_color_min)) {
    target_color = pulse_color_max;
  } else {
    target_color = pulse_color_min;
  }
  
  // Update transition timing and start transition timer
  transition_end_time = sl_sleeptimer_get_tick_count() + sl_sleeptimer_ms_to_tick(default_transition_duration_ms);
  sl_sleeptimer_restart_periodic_timer_ms(&color_transition_timer,
                                          transition_update_interval_ms,
                                          color_transition_callback,
                                          NULL,
                                          0,
                                          0);
}

// Public API Implementation

void led_effects_init(void)
{
  // Initialize colors to off
  rgb_t off_color = {0, 0, 0};
  current_color = off_color;
  target_color = off_color;
  pulse_active = false;
  
  // Apply initial state
  apply_current_color();
}

void led_effects_set_target_color(const rgb_t *color)
{
  // Stop pulse effect if running
  if (pulse_active) {
    sl_sleeptimer_stop_timer(&pulse_effect_timer);
    pulse_active = false;
  }
  
  target_color = *color;
  
  // If we're not already at this color, start transitioning
  if (!colors_equal(&current_color, &target_color)) {
    transition_end_time = sl_sleeptimer_get_tick_count() + sl_sleeptimer_ms_to_tick(default_transition_duration_ms);
    sl_sleeptimer_restart_periodic_timer_ms(&color_transition_timer,
                                            transition_update_interval_ms,
                                            color_transition_callback,
                                            NULL,
                                            0,
                                            0);
  }
}

void led_effects_set_color_immediate(const rgb_t *color)
{
  // Stop all effects
  pulse_active = false;
  sl_sleeptimer_stop_timer(&pulse_effect_timer);
  sl_sleeptimer_stop_timer(&color_transition_timer);
  
  // Set colors immediately
  current_color = *color;
  target_color = *color;
  apply_current_color();
}

void led_effects_start_pulse(void)
{
  // Stop any existing effects
  sl_sleeptimer_stop_timer(&pulse_effect_timer);
  sl_sleeptimer_stop_timer(&color_transition_timer);
  
  // Set initial state
  led_effects_set_color_immediate(&pulse_color_min);
  target_color = pulse_color_max;
  pulse_active = true;
  
  // Start the pulse toggle timer
  sl_sleeptimer_start_periodic_timer_ms(&pulse_effect_timer,
                                        pulse_toggle_interval_ms,
                                        pulse_effect_callback,
                                        NULL,
                                        0,
                                        0);
  
  // Trigger the first transition
  transition_end_time = sl_sleeptimer_get_tick_count() + sl_sleeptimer_ms_to_tick(default_transition_duration_ms);
  sl_sleeptimer_restart_periodic_timer_ms(&color_transition_timer,
                                          transition_update_interval_ms,
                                          color_transition_callback,
                                          NULL,
                                          0,
                                          0);
}

void led_effects_stop_pulse(void)
{
  pulse_active = false;
  sl_sleeptimer_stop_timer(&pulse_effect_timer);
  
  // Fade to off
  rgb_t off_color = {0, 0, 0};
  led_effects_set_target_color(&off_color);
}

void led_effects_stop_all(void)
{
  pulse_active = false;
  sl_sleeptimer_stop_timer(&pulse_effect_timer);
  sl_sleeptimer_stop_timer(&color_transition_timer);
  
  // Set to off immediately
  rgb_t off_color = {0, 0, 0};
  led_effects_set_color_immediate(&off_color);
}

bool led_effects_is_pulsing(void)
{
  return pulse_active;
}

rgb_t led_effects_get_current_color(void)
{
  return current_color;
}