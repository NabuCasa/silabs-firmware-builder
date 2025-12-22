#include "led_manager.h"
#include "led_effects_config.h"
#include "ws2812.h"
#include "sl_sleeptimer.h"
#include "sl_led.h"
#include "em_core.h"
#include <string.h>

// Internal state for each priority layer
typedef struct {
    led_pattern_t pattern;
    bool active;
    uint32_t start_tick;
    uint32_t expiry_tick;
} led_layer_state_t;

static led_layer_state_t layers[LED_PRIORITY_COUNT];
static sl_sleeptimer_timer_handle_t led_timer;
static volatile uint32_t global_tick_counter = 0;
static volatile bool update_needed = false;
static bool manager_initialized = false;

static void update_led_hardware(void) {
    int top_layer = -1;
    uint32_t current_tick = global_tick_counter;

    // Check for expirations and find highest active layer
    for (int i = LED_PRIORITY_COUNT - 1; i >= 0; i--) {
        if (layers[i].active) {
            // Handle auto-expiry for NOTIFICATION or other timed layers
            if (layers[i].pattern.duration_ms > 0 && current_tick >= layers[i].expiry_tick) {
                layers[i].active = false;
                continue;
            }
            if (top_layer == -1) {
                top_layer = i;
            }
        }
    }


    if (top_layer == -1) {
        sl_led_turn_off(&sl_led_ws2812.led_common);
        return;
    }

    led_layer_state_t *l = &layers[top_layer];
    led_pattern_t *p = &l->pattern;
    uint32_t layer_ticks = current_tick - l->start_tick;
    uint32_t ms_elapsed = layer_ticks * LED_EFFECTS_UPDATE_INTERVAL_MS;

    switch (p->mode) {
        case LED_MODE_OFF:
            sl_led_turn_off(&sl_led_ws2812.led_common);
            break;

        case LED_MODE_STATIC:
            sl_led_set_rgb_color(&sl_led_ws2812, p->color.r, p->color.g, p->color.b);
            sl_led_turn_on(&sl_led_ws2812.led_common);
            break;

        case LED_MODE_BLINK: {
            uint32_t period = (p->period_ms > 0) ? p->period_ms : 500;
            bool on = (ms_elapsed % period) < (period / 2);
            if (on) {
                sl_led_set_rgb_color(&sl_led_ws2812, p->color.r, p->color.g, p->color.b);
                sl_led_turn_on(&sl_led_ws2812.led_common);
            } else {
                sl_led_turn_off(&sl_led_ws2812.led_common);
            }
            break;
        }

        case LED_MODE_PULSE: {
            uint32_t period = (p->period_ms > 2) ? p->period_ms : 1000;
            uint32_t half_period = period / 2;
            uint32_t phase = ms_elapsed % period;
            
            uint32_t brightness; // 0 to 65535
            if (phase < half_period) {
                brightness = (phase * 65535) / half_period;
            } else {
                brightness = 65535 - ((phase - half_period) * 65535) / (period - half_period);
            }

            uint32_t min_b = 6553;
            brightness = min_b + ((65535 - min_b) * brightness / 65535);

            uint16_t r = ((uint32_t)p->color.r * brightness) / 65535;
            uint16_t g = ((uint32_t)p->color.g * brightness) / 65535;
            uint16_t b = ((uint32_t)p->color.b * brightness) / 65535;

            sl_led_set_rgb_color(&sl_led_ws2812, r, g, b);
            sl_led_turn_on(&sl_led_ws2812.led_common);
            break;
        }
    }
}

static void led_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data) {
    (void)handle;
    (void)data;
    global_tick_counter++;
    update_needed = true;
    ws2812_led_driver_refresh();
}

void led_manager_init(void) {
    if (manager_initialized) return;

    memset(layers, 0, sizeof(layers));
    
    sl_sleeptimer_start_periodic_timer_ms(&led_timer,
                                          LED_EFFECTS_UPDATE_INTERVAL_MS,
                                          led_timer_callback,
                                          NULL,
                                          0,
                                          0);
    manager_initialized = true;
}

void led_manager_process_action(void) {
    if (update_needed) {
        update_needed = false;
        update_led_hardware();
    }
}

void led_manager_set_pattern(led_priority_t priority, const led_pattern_t *pattern) {
    if (priority >= LED_PRIORITY_COUNT) return;

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();

    layers[priority].pattern = *pattern;
    layers[priority].active = true;
    layers[priority].start_tick = global_tick_counter;
    
    if (pattern->duration_ms > 0) {
        layers[priority].expiry_tick = global_tick_counter + (pattern->duration_ms / LED_EFFECTS_UPDATE_INTERVAL_MS);
    }

    CORE_EXIT_CRITICAL();
}

void led_manager_clear_pattern(led_priority_t priority) {
    if (priority >= LED_PRIORITY_COUNT) return;
    layers[priority].active = false;
}

void led_manager_set_color(led_priority_t priority, rgb_t color) {
    led_pattern_t p = {
        .mode = LED_MODE_STATIC,
        .color = color,
        .period_ms = 0,
        .duration_ms = 0
    };
    led_manager_set_pattern(priority, &p);
}
