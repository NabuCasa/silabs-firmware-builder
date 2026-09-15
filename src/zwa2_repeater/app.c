/**
 * Z-Wave Application Repeater
 *
 * A repeater application for the Home Assistant Connect ZWA-2, based on the
 * Z-Wave LED Bulb sample application.
 *
 * @copyright 2020 Silicon Laboratories Inc.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "MfgTokens.h"
#include "zpal_log.h"
#include "ZW_system_startup_api.h"
#include "ZAF_Common_helper.h"
#include "ZAF_Common_interface.h"
#include "ZAF_network_learn.h"
#include "events.h"
#include "zpal_watchdog.h"
#include "board_indicator.h"
#include "zw_region_config.h"
#include "ZAF_ApplicationEvents.h"
#include "zaf_event_distributor_soc.h"
#include "zpal_misc.h"
#include "zaf_protocol_config.h"
#include "ZAF_PrintAppInfo.h"
#include "ZAF_nvm_app.h"
#include "board_indicator_control.h"
#include "repeater_config_nvm.h"
#include "CC_ColorSwitch.h"
#include "cc_color_switch_config_api.h"
#include "cc_color_switch_io.h"
#include "zwave_identity.h"
#include "btl_interface.h"
#include "app_hw.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#ifdef SL_CATALOG_ZW_CLI_COMMON_PRESENT
#include "zw_cli_common.h"
#endif

static void ApplicationTask(SApplicationHandles* pAppHandles);

bool m_indicator_active_from_cc = false;

bool restore_color_switch_cc_state() {
  s_colorComponent* components = cc_color_switch_get_colorComponents();
  for (int i = 0; i < cc_color_switch_get_length_colorComponents(); i++) {
    bool result = cc_color_switch_read(i, &components[i]);
    if (!result) {
      return false;
    }
  }

  uint8_t red = ZAF_Actuator_GetCurrentValue(&components[0].obj);
  uint8_t green = ZAF_Actuator_GetCurrentValue(&components[1].obj);
  uint8_t blue = ZAF_Actuator_GetCurrentValue(&components[2].obj);

  if (red != 0 || green != 0 || blue != 0) {
    rgb_t color = {
        .R = red,
        .G = green,
        .B = blue
    };
    set_idle_color(&color);
  }

  return true;
}


/**
 * @brief See description for function prototype in ZW_basis_api.h.
 */
ZW_APPLICATION_STATUS ApplicationInit(__attribute__((unused)) zpal_reset_reason_t eResetReason)
{
  SRadioConfig_t* RadioConfig;
  zpal_radio_region_t regionMfg;

  zpal_watchdog_init();
  zpal_enable_watchdog(true);

  ZPAL_LOG_INFO(ZPAL_LOG_APP, "ApplicationInit eResetReason = %d\n", eResetReason);

  RadioConfig = zaf_get_radio_config();

  bool nvm_init_done = ZAF_nvm_app_init();

  // Mirror how the controller firmware handles radio settings,
  // namely using the mfg token only as a fallback.
  if (nvm_init_done && RepeaterConfigExists()) {
    ReadApplicationRfRegion(&RadioConfig->eRegion);
    ReadApplicationTxPowerlevel(&RadioConfig->iTxPowerLevelMax,
                                &RadioConfig->iTxPowerLevelAdjust);
    ReadApplicationMaxLRTxPwr(&RadioConfig->iTxPowerLevelMaxLR);
  } else {
    ZW_GetMfgTokenDataCountryFreq((void*) &regionMfg);
    if (isRfRegionValid(regionMfg)) {
      RadioConfig->eRegion = regionMfg;
    }

    if (nvm_init_done) {
      RepeaterConfigWriteDefaults(RadioConfig);
    }
  }

  // Repair/Reconcile the S2 identity before the protocol validates it.
  if (!zwave_identity_reconcile(ZAF_isLongRangeRegion(RadioConfig->eRegion))) {
    bootloader_rebootAndInstall();
    return APPLICATION_POWER_DOWN;
  }

  /*
   * Register the main application task.
   *
   * Attention: this is the only FreeRTOS task that can invoke the ZAF API.
   *
   * ZW_UserTask_CreateTask() can be used to create additional application tasks. See the
   * Sensor PIR application for an example use of ZW_UserTask_CreateTask().
   */
  __attribute__((unused)) bool bWasTaskCreated = ZW_ApplicationRegisterTask(
    ApplicationTask,
    EAPPLICATIONEVENT_ZWRX,
    EAPPLICATIONEVENT_ZWCOMMANDSTATUS,
    zaf_get_protocol_config()
    );
  assert(bWasTaskCreated);

  return (APPLICATION_RUNNING);
}

/**
 * A pointer to this function is passed to ZW_ApplicationRegisterTask() making it the FreeRTOS
 * application task.
 */
static void ApplicationTask(SApplicationHandles* pAppHandles)
{
  uint32_t unhandledEvents = 0;
  ZAF_Init(xTaskGetCurrentTaskHandle(), pAppHandles);

  ZAF_PrintAppInfo();

  // Enables the button through the zw_app_hw_init event
  app_hw_init();

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  // Add EM1 requirement to prevent the application from entering EM2 sleep mode.
  // The EUSART used by the WS2812 LED driver is only available in EM1.
  sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
#endif

  // Restore Color Switch CC state from NVM - if that fails, use the default idle color
  if (!restore_color_switch_cc_state()) {
    Board_IndicateStatus(BOARD_STATUS_IDLE);
  }

  // Initialize other NC-specific hardware

  /* Enter SmartStart*/
  /* Protocol will commence SmartStart only if the node is NOT already included in the network */
  ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_INCLUSION_SMARTSTART);

  // Wait for and process events
  ZPAL_LOG_DEBUG(ZPAL_LOG_APP, "Repeater Event processor Started\r\n");
  for (;;)
  {
    unhandledEvents = zaf_event_distributor_distribute();
    if (0 != unhandledEvents)
    {
      ZPAL_LOG_DEBUG(ZPAL_LOG_APP, "Unhandled Events: 0x%08lx\n", unhandledEvents);
#ifdef UNIT_TEST
      return;
#endif
    }
  }
}

/**
 * @brief The core state machine of this sample application.
 * @param event The event that triggered the call of zaf_event_distributor_app_event_manager.
 */
void zaf_event_distributor_app_event_manager(const uint8_t event)
{
  ZPAL_LOG_DEBUG(ZPAL_LOG_APP, "zaf_event_distributor_app_event_manager Ev: %d\r\n", event);

  switch (event)
  {
  case EVENT_APP_BOOTLOADER:
    bootloader_rebootAndInstall();
    break;

  default:
    // Unknown event - do nothing.
    break;
  }

#ifdef SL_CATALOG_ZW_CLI_COMMON_PRESENT
  cli_log_system_events(event);
#endif
}

// Gets called when the current color was changed through Color Switch CC
void cc_color_switch_cb(s_colorComponent *colorComponent)
{

  // Get the current color
  s_colorComponent *components = cc_color_switch_get_colorComponents();
  uint8_t red = ZAF_Actuator_GetTargetValue(&components[0].obj);
  uint8_t green = ZAF_Actuator_GetTargetValue(&components[1].obj);
  uint8_t blue = ZAF_Actuator_GetTargetValue(&components[2].obj);

  // And merge it with the new value
  uint8_t new_value = ZAF_Actuator_GetTargetValue(&colorComponent->obj);
  switch (colorComponent->colorId)
  {
  case ECOLORCOMPONENT_RED:
    red = new_value;
    break;
  case ECOLORCOMPONENT_GREEN:
    green = new_value;
    break;
  case ECOLORCOMPONENT_BLUE:
    blue = new_value;
    break;
  default:
    // Unsupported color changed
    return;
  }

  rgb_t color = {
      green, red, blue};
  indicator_solid(&color);
}
