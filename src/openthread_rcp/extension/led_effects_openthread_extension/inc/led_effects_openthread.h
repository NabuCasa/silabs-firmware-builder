/*
 * led_effects_openthread.h
 *
 * LED State Machine for Network Status and Tilt Detection
 * Simple state-based LED control for adapter status indication
 */

#ifndef LED_EFFECTS_OPENTHREAD_H_
#define LED_EFFECTS_OPENTHREAD_H_

#include <stdbool.h>

/**
 * @brief Check if device has valid stored network configuration
 * @return true if network settings are stored, false otherwise
 */
bool device_has_stored_network_settings(void);

/**
 * @brief Main loop process action handler
 * Polls the OpenThread network state outside of interrupt context.
 */
void led_effects_openthread_process_action(void);

#endif /* LED_EFFECTS_OPENTHREAD_H_ */