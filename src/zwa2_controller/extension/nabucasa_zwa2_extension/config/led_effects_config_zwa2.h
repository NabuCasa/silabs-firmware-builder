/***************************************************************************//**
 * @file
 * @brief Configuration header for LED Effects (ZWA-2)
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

// <o LED_EFFECTS_TILT_THRESHOLD_UPPER> Tilt entry threshold (degrees)
// <i> Angle from vertical to enter tilted state
// <d> 16
#ifndef LED_EFFECTS_TILT_THRESHOLD_UPPER
#define LED_EFFECTS_TILT_THRESHOLD_UPPER  16.0f
#endif

// <o LED_EFFECTS_TILT_THRESHOLD_LOWER> Tilt exit threshold (degrees)
// <i> Angle from vertical to exit tilted state
// <d> 12
#ifndef LED_EFFECTS_TILT_THRESHOLD_LOWER
#define LED_EFFECTS_TILT_THRESHOLD_LOWER  12.0f
#endif

// <o LED_EFFECTS_TILT_STABLE_COUNT> Stable readings before tilt decision
// <i> Number of consecutive stable accelerometer readings required
// <d> 3
#ifndef LED_EFFECTS_TILT_STABLE_COUNT
#define LED_EFFECTS_TILT_STABLE_COUNT     3
#endif

// <o LED_EFFECTS_TILT_STABLE_DELTA> Max raw delta for stable reading
// <i> Maximum per-axis raw accelerometer difference for a reading to count as stable
// <d> 25
#ifndef LED_EFFECTS_TILT_STABLE_DELTA
#define LED_EFFECTS_TILT_STABLE_DELTA     25
#endif

// <o LED_EFFECTS_TILT_SAMPLE_MS> Tilt sample interval (ms)
// <i> How often to read the accelerometer for tilt detection
// <d> 100
#ifndef LED_EFFECTS_TILT_SAMPLE_MS
#define LED_EFFECTS_TILT_SAMPLE_MS        100
#endif

// <<< sl:end pin_tool >>>

#endif // LED_EFFECTS_CONFIG_H_
