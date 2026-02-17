/*
 * zwa2_button.h
 *
 * ZWA-2 Button Handler
 * Hooks into the SDK's app_button_press system with a weak callback
 */

#ifndef ZWA2_BUTTON_H
#define ZWA2_BUTTON_H

#include <stdint.h>

/**
 * @brief Handle pin-hole button press
 * @param duration Press duration from app_button_press (APP_BUTTON_PRESS_DURATION_*)
 *
 * Default implementation does nothing (weak symbol).
 * Override in application code for custom behavior.
 */
void zwa2_button_handle_press(uint8_t duration);

#endif // ZWA2_BUTTON_H
