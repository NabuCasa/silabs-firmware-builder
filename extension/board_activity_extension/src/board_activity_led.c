#include <stdbool.h>

#include "sl_led.h"
#include "sl_simple_led_instances.h"

void __wrap_halStackIndicateActivity(bool turnOn)
{
    if (turnOn) {
        sl_led_turn_on(&sl_led_board_activity);
    } else {
        sl_led_turn_off(&sl_led_board_activity);
    }
}
