/*
 * zwa2_button.c
 *
 * ZWA-2 Button Implementation
 * Overrides the SDK's weak app_button_press_btn_0_handler to dispatch
 * to a weak zwa2_button_handle_press that alternative firmwares can override.
 */

#include "zwa2_button.h"
#include "sl_common.h"

SL_WEAK void zwa2_button_handle_press(uint8_t duration)
{
    (void)duration;
}

void app_button_press_btn_0_handler(uint8_t duration)
{
    zwa2_button_handle_press(duration);
}
