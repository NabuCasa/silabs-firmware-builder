/***************************************************************************//**
 * @file
 * @brief Configuration header for LED Effects
 ******************************************************************************/
#ifndef LED_EFFECTS_CONFIG_H_
#define LED_EFFECTS_CONFIG_H_

// <<< sl:start pin_tool >>>

// <o LED_EFFECTS_UPDATE_INTERVAL_MS> LED update interval (ms)
// <i> Timer interval for LED animation updates
// <d> 4
#ifndef LED_EFFECTS_UPDATE_INTERVAL_MS
#define LED_EFFECTS_UPDATE_INTERVAL_MS    4
#endif

// <o LED_EFFECTS_TILT_THRESHOLD_DEG> Tilt threshold (degrees)
// <i> Angle threshold to trigger tilt detection
// <d> 16
#ifndef LED_EFFECTS_TILT_THRESHOLD_DEG
#define LED_EFFECTS_TILT_THRESHOLD_DEG    16
#endif

// <o LED_EFFECTS_TILT_HYSTERESIS_DEG> Tilt hysteresis (degrees)
// <i> Hysteresis to prevent flicker at threshold boundary
// <d> 4
#ifndef LED_EFFECTS_TILT_HYSTERESIS_DEG
#define LED_EFFECTS_TILT_HYSTERESIS_DEG   4
#endif

// <<< sl:end pin_tool >>>

#endif // LED_EFFECTS_CONFIG_H_
