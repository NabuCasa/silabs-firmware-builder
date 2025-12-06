/***************************************************************************//**
 * @file
 * @brief Configuration header for ZBT-2 Reset Button
 ******************************************************************************/
#ifndef ZBT2_RESET_BUTTON_CONFIG_H_
#define ZBT2_RESET_BUTTON_CONFIG_H_

// <<< sl:start pin_tool >>>

// <o ZBT2_RESET_BUTTON_CYCLES> Reset cycles required
// <i> Number of blink cycles the button must be held to trigger reset
// <d> 4
#ifndef ZBT2_RESET_BUTTON_CYCLES
#define ZBT2_RESET_BUTTON_CYCLES    4
#endif

// <o ZBT2_RESET_BUTTON_CYCLE_DELAY_MS> Cycle delay (ms)
// <i> Delay between reset cycles
// <d> 500
#ifndef ZBT2_RESET_BUTTON_CYCLE_DELAY_MS
#define ZBT2_RESET_BUTTON_CYCLE_DELAY_MS    500
#endif

// <o ZBT2_RESET_BUTTON_BLINK_ON_MS> Blink on duration (ms)
// <i> Duration LED stays on during blink
// <d> 150
#ifndef ZBT2_RESET_BUTTON_BLINK_ON_MS
#define ZBT2_RESET_BUTTON_BLINK_ON_MS    150
#endif

// <o ZBT2_RESET_BUTTON_BLINK_OFF_MS> Blink off duration (ms)
// <i> Duration LED stays off between blinks
// <d> 50
#ifndef ZBT2_RESET_BUTTON_BLINK_OFF_MS
#define ZBT2_RESET_BUTTON_BLINK_OFF_MS    50
#endif

// <o ZBT2_RESET_BUTTON_BLINK_START_DELAY_MS> Blink start delay (ms)
// <i> Delay before starting blink pattern after cycle increment
// <d> 200
#ifndef ZBT2_RESET_BUTTON_BLINK_START_DELAY_MS
#define ZBT2_RESET_BUTTON_BLINK_START_DELAY_MS    200
#endif

// <<< sl:end pin_tool >>>

#endif // ZBT2_RESET_BUTTON_CONFIG_H_
