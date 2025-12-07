/*
 * led_effects.h
 *
 * LED State Machine for Network Status and Tilt Detection
 * Simple state-based LED control for adapter status indication
 */

#ifndef LED_EFFECTS_H_
#define LED_EFFECTS_H_

#include <stdint.h>
#include <stdbool.h>
#include "ws2812.h"
#include "sl_sleeptimer.h"

#define M_PI  3.14159265358979323846f

// LED State Machine
typedef enum {
  LED_STATE_NETWORK_FORMED,      // Network connected - LEDs off
  LED_STATE_NETWORK_NOT_FORMED,  // No network - fading between on/off 
  LED_STATE_TILTED               // Device tilted >10° - 250ms on/off blinking
} led_state_t;

// LED Effects API

/**
 * @brief Initialize the LED effects system
 * Must be called after initWs2812() and initqma6100p() during system initialization
 */
void led_effects_init(void);

/**
 * @brief Set network formation state
 * @param network_formed true if network is formed, false otherwise
 */
void led_effects_set_network_state(bool network_formed);

/**
 * @brief Stop all LED effects immediately
 */
void led_effects_stop_all(void);

/**
 * @brief Get current LED state
 * @return Current LED state
 */
led_state_t led_effects_get_state(void);

/**
 * @brief Check if device has valid stored network configuration
 * @return true if network settings are stored, false otherwise
 */
extern bool device_has_stored_network_settings(void);

#endif /* LED_EFFECTS_H_ */