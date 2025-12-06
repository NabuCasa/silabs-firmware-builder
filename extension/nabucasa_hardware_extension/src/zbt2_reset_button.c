/*
 * zbt2_reset_button.c
 *
 * ZBT-2 Reset Button Implementation
 * Handles pin-hole button press to factory reset the Zigbee network
 */

#include "zbt2_reset_button.h"
#include "zbt2_reset_button_config.h"
#include "led_effects.h"
#include "ws2812.h"

#include "hal/hal.h"
#include "ember.h"
#include "sl_token_manager.h"
#include "sl_sleeptimer.h"
#include "sl_button.h"
#include "sl_simple_button_instances.h"

static sl_sleeptimer_timer_handle_t reset_timer;
static sl_sleeptimer_timer_handle_t blink_timer;

static uint8_t reset_cycle = 0;
static uint8_t blink_count = 0;
static bool led_on = false;

// Task to handle the LED blinking pattern
static void blink_task(sl_sleeptimer_timer_handle_t *handle, void *data);

// Resets the Zigbee network settings and reboots the adapter
static void reset_adapter(void)
{
    // Set the LED to red to indicate that the reset is in progress.
    set_all_leds(&red);

    // Erase NVRAM
    sl_token_init();
    sl_zigbee_token_factory_reset(false, false);

    halReboot();
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

    // If the LED is on, turn it off
    if (led_on) {
        set_all_leds(&off);
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
        // If the LED is off, turn it on
        set_all_leds(&amber);
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
        led_effects_stop_all();
        reset_cycle = 0;
        blink_count = 0;
        led_on = false;

        sl_sleeptimer_start_timer_ms(&reset_timer, ZBT2_RESET_BUTTON_CYCLE_DELAY_MS, reset_timer_callback, NULL, 0, 0);
    } else {
        // This is the release and will only be hit if we cancel early
        led_effects_set_network_state(device_has_stored_network_settings());
    }
}

// SDK callback: button state changed
void sl_button_on_change(const sl_button_t *handle)
{
    if (handle == &sl_button_pin_hole_button) {
        bool pressed = (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED);
        zbt2_reset_button_handle_state(pressed);
    }
}
