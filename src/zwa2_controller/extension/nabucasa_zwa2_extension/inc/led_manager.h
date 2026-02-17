#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "led_manager_colors.h"

// Priorities for LED control (Higher value = Higher priority)
typedef enum {
    LED_PRIORITY_BACKGROUND = 0, // Network state (pulsing, off)
    LED_PRIORITY_MANUAL,         // User command (the "Backup" color)
    LED_PRIORITY_TILT,           // Tilt detection blink
    LED_PRIORITY_SYSTEM,         // System indications (warn, error)
    LED_PRIORITY_COUNT
} led_priority_t;

// Animation modes
typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_STATIC,    // Constant color
    LED_MODE_BLINK,     // On/Off square wave
    LED_MODE_PULSE,     // Triangle wave fading
} led_mode_t;

typedef struct {
    led_mode_t mode;
    rgb_t color;
    uint16_t period_ms;      // Cycle time for blink/pulse
    uint32_t duration_ms;    // Auto-clear after this time (0 = infinite)
    uint16_t brightness_min; // Min brightness for pulse (0-65535)
    uint16_t brightness_max; // Max brightness for pulse (0-65535)
} led_pattern_t;

/**
 * @brief Initialize the LED manager
 */
void led_manager_init(void);

/**
 * @brief Main loop process action handler
 * Handles LED updates safely outside of interrupt context.
 */
void led_manager_process_action(void);

/**
 * @brief Set a pattern for a specific priority layer
 * @param priority The priority layer to set
 * @param pattern The pattern to apply.
 */
void led_manager_set_pattern(led_priority_t priority, const led_pattern_t *pattern);

/**
 * @brief Clear the pattern for a specific priority layer
 * @param priority The priority layer to clear
 */
void led_manager_clear_pattern(led_priority_t priority);

/**
 * @brief Helper to set a static color on a layer
 */
void led_manager_set_color(led_priority_t priority, rgb_t color);

#endif // LED_MANAGER_H
