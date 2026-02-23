/*
 * led_effects_zwa2.h
 *
 * LED System Behaviors for ZWA-2 Z-Wave Controller
 * High-level control for autonomous LED behaviors (Network Status, Tilt)
 */

#ifndef LED_EFFECTS_H_
#define LED_EFFECTS_H_

#include <stdbool.h>

/**
 * @brief Initialize the LED effects system (and the underlying manager)
 * Starts the tilt monitor.
 */
void led_effects_init(void);

/**
 * @brief Set network searching state (pulse white on BACKGROUND)
 * Called at startup before host connects.
 */
void led_effects_set_searching(void);

/**
 * @brief Set network connected state (solid white on BACKGROUND)
 * Called when host sends the first serial command.
 */
void led_effects_set_connected(void);

/**
 * @brief Enable or disable tilt detection
 * @param enabled true to enable tilt blink, false to disable
 */
void led_effects_set_tilt_enabled(bool enabled);

#endif /* LED_EFFECTS_H_ */
