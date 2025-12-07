/*
 * zbt2_reset_button.h
 *
 * ZBT-2 Reset Button Handler
 * Pin-hole button press handler for factory reset
 */

#ifndef ZBT2_RESET_BUTTON_H
#define ZBT2_RESET_BUTTON_H

#include <stdbool.h>

/**
 * @brief Handle button state change
 * @param pressed true if button is pressed, false if released
 */
void zbt2_reset_button_handle_state(bool pressed);

#endif // ZBT2_RESET_BUTTON_H
