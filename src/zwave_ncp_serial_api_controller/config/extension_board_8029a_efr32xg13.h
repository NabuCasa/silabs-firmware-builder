/**
 * Provides support for BRD8029A (Buttons and LEDs EXP Board)
 *
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef EXTENSION_BOARD_8029A_EFR32XG13_H
#define EXTENSION_BOARD_8029A_EFR32XG13_H

#include "extension_board_8029a_efr32xg13_button.h"
#include "extension_board_8029a_efr32xg13_led.h"
#include "extension_board_8029a_efr32xg13_slider.h"

/*************************************************************************/
/* Map physical board IO devices to application LEDs and buttons         */
/*************************************************************************/

/* Map application LEDs to board LEDs */
#define APP_LED_A              BOARD_LED1
#define APP_LED_INDICATOR      BOARD_LED2  // Positioned opposite APP_BUTTON_LEARN_RESET
#define APP_LED_B              BOARD_LED4
#define APP_LED_C              BOARD_LED3

#define APP_RGB_R              BOARD_RGB1_R
#define APP_RGB_G              BOARD_RGB1_G
#define APP_RGB_B              BOARD_RGB1_B

/* Mapping application buttons to board buttons */
#if defined(RADIO_BOARD_EFR32ZG13P32) || defined(RADIO_BOARD_EFR32ZG13L) || defined(RADIO_BOARD_EFR32ZG13S)
// The EFR32ZG13P32 device has reduced number of GPIO pins and therefore
// supports only two of our buttons
#define APP_BUTTON_A           BOARD_BUTTON_PB4
#define APP_BUTTON_LEARN_RESET BOARD_BUTTON_PB3  // Supports EM4 wakeup
#else
#define APP_BUTTON_A           BOARD_BUTTON_PB1
#define APP_BUTTON_LEARN_RESET BOARD_BUTTON_PB2  // Supports EM4 wakeup
#define APP_BUTTON_B           BOARD_BUTTON_PB3  // Supports EM4 wakeup
#define APP_BUTTON_C           BOARD_BUTTON_PB4
#define APP_SLIDER_A           BOARD_BUTTON_SLIDER1
#endif

/* The next two are identical since on the BRD8029A only PB2 and PB3
 * can trigger a wakeup from EM4. PB2 is already used for learn/reset
 */
#define APP_WAKEUP_BTN_SLDR    BOARD_BUTTON_PB3 // Use this one when wakeup capability is required and button is preferred to slider
#define APP_WAKEUP_SLDR_BTN    BOARD_BUTTON_PB3 // Use this one when wakeup capability is required and slider is preferred to button

#endif /* EXTENSION_BOARD_8029A_EFR32XG13_H */