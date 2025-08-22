/*
 * led_effects.h
 *
 * LED Color Transition and Effects System
 * Provides smooth color transitions and effects for WS2812 LED strips
 */

#ifndef LED_EFFECTS_H_
#define LED_EFFECTS_H_

#include <stdint.h>
#include <stdbool.h>
#include "ws2812.h"
#include "sl_sleeptimer.h"

// LED Effects API

/**
 * @brief Initialize the LED effects system
 * Must be called after initWs2812() during system initialization
 */
void led_effects_init(void);

/**
 * @brief Set target color with smooth transition
 * @param color Target color to transition to
 */
void led_effects_set_target_color(const rgb_t *color);

/**
 * @brief Set color immediately without transition
 * @param color Color to set immediately
 */
void led_effects_set_color_immediate(const rgb_t *color);

/**
 * @brief Start pulsing effect between two colors
 * Uses the configured pulse colors (min/max)
 */
void led_effects_start_pulse(void);

/**
 * @brief Stop pulsing effect and fade to off
 */
void led_effects_stop_pulse(void);

/**
 * @brief Stop all effects and set LEDs off immediately
 */
void led_effects_stop_all(void);

/**
 * @brief Check if pulsing effect is currently active
 * @return true if pulsing, false otherwise
 */
bool led_effects_is_pulsing(void);

/**
 * @brief Get current LED color
 * @return Current color being displayed
 */
rgb_t led_effects_get_current_color(void);

#endif /* LED_EFFECTS_H_ */