/*
 * led_effects_zigbee.c
 *
 * LED State Machine Implementation
 * Simple state-based LED control for adapter status indication
 */

#include "led_effects_zigbee.h"
#include "sl_token_api.h"
#include "stack/include/ember-types.h"
#include "led_effects.h"

// Check if device has valid stored network configuration
bool device_has_stored_network_settings(void)
{
  tokTypeStackNodeData nodeData;
  halCommonGetToken(&nodeData, TOKEN_STACK_NODE_DATA);

  if (nodeData.panId == 0xFFFF) {
    return false;
  }

  if (nodeData.radioFreqChannel < 11 || nodeData.radioFreqChannel > 26) {
    return false;
  }

  return true;
}

// SDK callback: stack status change
void led_effects_stack_status_callback(EmberStatus status)
{
  (void)status;
  led_effects_set_network_state(device_has_stored_network_settings());
}

void led_effects_system_init(uint8_t init_level)
{
  (void)init_level;
  led_effects_init();
  led_effects_set_network_state(device_has_stored_network_settings());
}
