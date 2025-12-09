/***************************************************************************//**
 * @file
 * @brief Configuration header for Board Activity LED
 ******************************************************************************/
#ifndef BOARD_ACTIVITY_LED_CONFIG_H_
#define BOARD_ACTIVITY_LED_CONFIG_H_

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Board Activity LED Configuration

// <o BOARD_ACTIVITY_LED_INSTANCE> LED instance for stack activity
// <i> The simple_led instance to use for stack activity indication
#ifndef BOARD_ACTIVITY_LED_INSTANCE
#warning "Board activity LED not configured"
// #define BOARD_ACTIVITY_LED_INSTANCE    sl_led_board_activity
#endif

// </h>

// <<< end of configuration section >>>

#endif // BOARD_ACTIVITY_LED_CONFIG_H_
