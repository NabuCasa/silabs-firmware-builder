/*
 * zbt2_reset_button.c
 *
 * ZBT-2 Reset Button Implementation
 * Handles pin-hole button press to factory reset network settings
 */

#include "zbt2_reset_button.h"
#include "zbt2_reset_button_config.h"
#include "led_manager.h"

#include "em_core.h"
#include "sl_sleeptimer.h"

#include "sl_button.h"
#include "sl_simple_button_instances.h"

#if ZBT2_RESET_BUTTON_ERASE_COUNTERS
#include "nvm3_default.h"
#include "psa/crypto.h"

#define ZB_PSA_KEY_ID_MIN  0x00030000
#define ZB_PSA_KEY_ID_MAX  0x0003FFFF
#else
#include "stack-info.h"
#endif

static sl_sleeptimer_timer_handle_t reset_timer;
static sl_sleeptimer_timer_handle_t blink_timer;

static uint8_t reset_cycle = 0;
static uint8_t blink_count = 0;
static bool led_on = false;

// Task to handle the LED blinking pattern
static void blink_task(sl_sleeptimer_timer_handle_t *handle, void *data);

// Resets network settings and reboots the adapter
static void reset_adapter(void)
{
#if ZBT2_RESET_BUTTON_ERASE_COUNTERS
    // Full NVM3 erase
    led_manager_set_color(LED_PRIORITY_CRITICAL, LED_COLOR_RESET_RED);

    nvm3_initDefault();
    nvm3_eraseAll(nvm3_defaultHandle);

    for (psa_key_id_t key_id = ZB_PSA_KEY_ID_MIN; key_id <= ZB_PSA_KEY_ID_MAX; key_id++) {
        psa_destroy_key(key_id);
    }
#else
    led_manager_set_color(LED_PRIORITY_CRITICAL, LED_COLOR_RESET_ORANGE);

    // Zigbee token reset - preserves frame counters and boot counter
    sl_zigbee_token_factory_reset(true, true);
#endif

    NVIC_SystemReset();
}

// Called when the reset timer expires.
static void reset_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)handle;
    (void)data;

    reset_cycle++;
    sl_sleeptimer_start_timer_ms(&blink_timer, ZBT2_RESET_BUTTON_BLINK_START_DELAY_MS, blink_task, NULL, 0, 0);
}

// Task to handle the LED blinking pattern.
static void blink_task(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)handle;
    (void)data;

    // If the LED is on, turn it off (clear the layer to reveal previous state)
    if (led_on) {
        led_manager_clear_pattern(LED_PRIORITY_CRITICAL);
        led_on = false;

        // If we have blinked enough times, then start the next reset cycle
        if (blink_count >= reset_cycle) {
            blink_count = 0;
            if (reset_cycle == ZBT2_RESET_BUTTON_CYCLES) {
                reset_adapter();
            } else {
                sl_sleeptimer_start_timer_ms(&reset_timer, ZBT2_RESET_BUTTON_CYCLE_DELAY_MS, reset_timer_callback, NULL, 0, 0);
            }
        } else {
            sl_sleeptimer_start_timer_ms(&blink_timer, ZBT2_RESET_BUTTON_BLINK_OFF_MS, blink_task, NULL, 0, 0);
        }
    } else {
        led_manager_set_color(LED_PRIORITY_CRITICAL, LED_COLOR_RESET_ORANGE);
        led_on = true;
        blink_count++;
        sl_sleeptimer_start_timer_ms(&blink_timer, ZBT2_RESET_BUTTON_BLINK_ON_MS, blink_task, NULL, 0, 0);
    }
}

void zbt2_reset_button_handle_state(bool pressed)
{
    // Stop any running timers first
    sl_sleeptimer_stop_timer(&reset_timer);
    sl_sleeptimer_stop_timer(&blink_timer);

    if (pressed) {
        // Reset state and start the cycle, this is hit only when the button is initially pressed
        reset_cycle = 0;
        blink_count = 0;
        led_on = false;

        sl_sleeptimer_start_timer_ms(&reset_timer, ZBT2_RESET_BUTTON_CYCLE_DELAY_MS, reset_timer_callback, NULL, 0, 0);
    } else {
        // This is the release and will only be hit if we cancel early.
        led_manager_clear_pattern(LED_PRIORITY_CRITICAL);
        led_on = false;
    }
}

void sl_button_on_change(const sl_button_t *handle)
{
    if (handle == &sl_button_pin_hole_button) {
        bool pressed = (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED);
        zbt2_reset_button_handle_state(pressed);
    }
}
