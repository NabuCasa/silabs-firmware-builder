/*
 * led_effects.h
 *
 * LED System Behaviors
 * High-level control for autonomous LED behaviors (Network Status, Tilt)
 */

#ifndef LED_EFFECTS_H_
#define LED_EFFECTS_H_

#include <stdbool.h>

/**
 * @brief Initialize the LED effects system (and the underlying manager)
 * Starts the tilt monitor and network status indication.
 */
void led_effects_init(void);

/**
 * @brief Set network formation state
 * Updates the BACKGROUND priority layer.
 * @param network_formed true if network is formed (LED Off), false otherwise (Pulse)
 */
void led_effects_set_network_state(bool network_formed);

/**
 * @brief Check if device has valid stored network configuration
 * Implemented by the application (Router/NCP/OT)
 * @return true if network settings are stored, false otherwise
 */
extern bool device_has_stored_network_settings(void);

#endif /* LED_EFFECTS_H_ */
