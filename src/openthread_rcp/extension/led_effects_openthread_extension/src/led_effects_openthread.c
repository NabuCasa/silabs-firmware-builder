/*
 * led_effects_openthread.c
 *
 * LED State Machine Implementation
 * Simple state-based LED control for adapter status indication
 */

#include <openthread/instance.h>
#include <openthread/link.h>
#include <openthread/platform/radio.h>

#include "led_effects_openthread.h"
#include "led_effects.h"
#include "sl_sleeptimer.h"

#define SETTINGS_POLL_INTERVAL_MS 250

extern otInstance *otGetInstance(void);

static bool network_has_settings = false;
static sl_sleeptimer_timer_handle_t pan_id_poll_timer;

static void network_state_poll_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;

  otInstance *instance = otGetInstance();
  otPanId pan_id = otLinkGetPanId(instance);
  bool has_network = (pan_id != 0xFFFF) && otPlatRadioIsEnabled(instance);

  if (has_network != network_has_settings) {
    network_has_settings = has_network;
    led_effects_set_network_state(network_has_settings);
  }
}

bool device_has_stored_network_settings(void)
{
  return network_has_settings;
}

void led_effects_system_init(void)
{
  led_effects_init();
  led_effects_set_network_state(device_has_stored_network_settings());

  sl_sleeptimer_start_periodic_timer_ms(&pan_id_poll_timer,
                                        SETTINGS_POLL_INTERVAL_MS,
                                        network_state_poll_callback,
                                        NULL,
                                        0,
                                        0);
}
