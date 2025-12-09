#include <stdbool.h>

#include "board_activity_led_config.h"
#include "sl_led.h"
#include BOARD_ACTIVITY_LED_INCLUDE

void __wrap_halStackIndicateActivity(bool turnOn)
{
    if (turnOn) {
        sl_led_turn_on((const sl_led_t *)&BOARD_ACTIVITY_LED_INSTANCE);
    } else {
        sl_led_turn_off((const sl_led_t *)&BOARD_ACTIVITY_LED_INSTANCE);
    }
}
