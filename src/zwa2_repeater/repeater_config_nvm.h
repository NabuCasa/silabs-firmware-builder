#ifndef REPEATER_CONFIG_NVM_H
#define REPEATER_CONFIG_NVM_H

#include <stdbool.h>
#include <stdint.h>
#include "ZW_application_transport_interface.h"

bool RepeaterConfigExists(void);
bool RepeaterConfigWriteDefaults(const SRadioConfig_t *radio_config);

bool SaveApplicationRfRegion(zpal_radio_region_t rf_region);
bool ReadApplicationRfRegion(zpal_radio_region_t *rf_region);

bool SaveApplicationTxPowerlevel(zpal_tx_power_decidbm_t tx_power_level,
                                 zpal_tx_power_decidbm_t tx_power_adjust);
bool ReadApplicationTxPowerlevel(zpal_tx_power_decidbm_t *tx_power_level,
                                 zpal_tx_power_decidbm_t *tx_power_adjust);

bool SaveApplicationMaxLRTxPwr(zpal_tx_power_decidbm_t max_tx_power_lr);
bool ReadApplicationMaxLRTxPwr(zpal_tx_power_decidbm_t *max_tx_power_lr);

#endif