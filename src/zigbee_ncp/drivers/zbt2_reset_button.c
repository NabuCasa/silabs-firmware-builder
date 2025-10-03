#include "hal/hal.h"
#include "ember.h"

#include "zbt2_reset_button.h"

#include "sl_simple_button_pin_hole_button_config.h"
#include "sl_simple_button_instances.h"
#include "sl_sleeptimer.h"
#include "ws2812.h"

// The number of cycles the button must be held to trigger a reset.
#define RESET_CYCLES 4

// Animation delays in milliseconds.
#define CYCLE_DELAY_MS 500
#define BLINK_ON_DELAY_MS 150
#define BLINK_OFF_DELAY_MS 50
#define RESET_CONFIRMATION_DELAY_MS 1000

// Timer handles
static sl_sleeptimer_timer_handle_t reset_timer;
static sl_sleeptimer_timer_handle_t blink_timer;

// The current reset cycle.
static uint8_t reset_cycle = 0;

// Task to handle the LED blinking pattern.
static void blink_task(sl_sleeptimer_timer_handle_t *handle, void *data);

// Resets the Zigbee network settings and reboots the adapter.
static void reset_adapter(void)
{
    // Set the LED to red to indicate that the reset is in progress.
    set_all_leds(&red);

    // Erase NVRAM
    sl_token_init();
    sl_zigbee_token_factory_reset(false, false);

    // Synchronously wait for a while and then reboot
    halReboot();
}

// Called when the reset timer expires.
static void reset_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    // Increment the reset cycle.
    reset_cycle++;

    // Start the blink task.
    sl_sleeptimer_start_timer_ms(&blink_timer, 200, blink_task, NULL, 0, 0);
}

// Task to handle the LED blinking pattern.
static void blink_task(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    static uint8_t blink_count = 0;
    static bool led_on = false;

    // If the LED is on, turn it off.
    if (led_on) {
        set_all_leds(&off);
        led_on = false;

        // If we have blinked enough times, then start the next reset cycle.
        if (blink_count >= reset_cycle) {
            blink_count = 0;
            if (reset_cycle == RESET_CYCLES) {
                reset_adapter();
            } else {
                sl_sleeptimer_start_timer_ms(&reset_timer, CYCLE_DELAY_MS, reset_timer_callback, NULL, 0, 0);
            }
        } else {
            sl_sleeptimer_start_timer_ms(&blink_timer, BLINK_OFF_DELAY_MS, blink_task, NULL, 0, 0);
        }
    } else {
        // If the LED is off, turn it on.
        set_all_leds(&amber);
        led_on = true;
        blink_count++;
        sl_sleeptimer_start_timer_ms(&blink_timer, BLINK_ON_DELAY_MS, blink_task, NULL, 0, 0);
    }
}

// Called when the button changes state.
void sl_button_on_change(const sl_button_t *handle)
{
    // If the button is the pinhole button, then handle the state change.
    if (handle == &sl_button_pin_hole_button) {
        // If the button is pressed, start the reset timer.
        if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
            reset_cycle = 0;
            sl_sleeptimer_start_timer_ms(&reset_timer, CYCLE_DELAY_MS, reset_timer_callback, NULL, 0, 0);
        } else {
            // If the button is released, stop the reset timer and the blink timer.
            sl_sleeptimer_stop_timer(&reset_timer);
            sl_sleeptimer_stop_timer(&blink_timer);

            // Turn the LED off.
            set_all_leds(&off);
        }
    }
}

// Initializes the reset button.
void zbt2_reset_button_init(void)
{
    // The simple_button component handles the initialization of the button.
}
