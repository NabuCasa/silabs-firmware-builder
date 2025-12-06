/*
 * led_effects_openthread.c
 *
 * LED State Machine Implementation
 * Simple state-based LED control for adapter status indication
 */

#include "led_effects_openthread.h"
#include "led_effects.h"

// Check if device has valid stored network configuration
bool device_has_stored_network_settings(void)
{
  return false;
}

void led_effects_system_init()
{
  led_effects_init();
  led_effects_set_network_state(device_has_stored_network_settings());
}
