#include "repeater_config_nvm.h"

#include <string.h>

#include "ZAF_nvm_app.h"

#define FILE_ID_APPLICATIONCONFIGURATION  104

typedef struct __attribute__((packed)) {
  zpal_radio_region_t     rfRegion;
  zpal_tx_power_decidbm_t         iTxPower;
  zpal_tx_power_decidbm_t         ipower0dbmMeasured;
  zpal_tx_power_decidbm_t         maxTxPower;
} SApplicationConfiguration;

static bool object_exists(zpal_nvm_object_key_t key)
{
  size_t object_size = 0;

  return (ZPAL_STATUS_OK == ZAF_nvm_app_get_object_size(key, &object_size));
}

static bool read_application_configuration(SApplicationConfiguration *configuration)
{
  if (!object_exists(FILE_ID_APPLICATIONCONFIGURATION)) {
    return false;
  }

  return (ZPAL_STATUS_OK == ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION,
                                             configuration,
                                             sizeof(*configuration)));
}

static bool write_application_configuration(const SApplicationConfiguration *configuration)
{
  return (ZPAL_STATUS_OK == ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION,
                                              configuration,
                                              sizeof(*configuration)));
}

bool RepeaterConfigExists(void)
{
  return object_exists(FILE_ID_APPLICATIONCONFIGURATION);
}

bool RepeaterConfigWriteDefaults(const SRadioConfig_t *radio_config)
{
  SApplicationConfiguration configuration;

  memset(&configuration, 0, sizeof(configuration));
  configuration.rfRegion = radio_config->eRegion;
  configuration.iTxPower = radio_config->iTxPowerLevelMax;
  configuration.ipower0dbmMeasured = radio_config->iTxPowerLevelAdjust;
  configuration.maxTxPower = radio_config->iTxPowerLevelMaxLR;

  return write_application_configuration(&configuration);
}

bool SaveApplicationRfRegion(zpal_radio_region_t rf_region)
{
  SApplicationConfiguration configuration;

  if (!read_application_configuration(&configuration)) {
    return false;
  }

  configuration.rfRegion = rf_region;
  return write_application_configuration(&configuration);
}

bool ReadApplicationRfRegion(zpal_radio_region_t *rf_region)
{
  SApplicationConfiguration configuration;

  if (!read_application_configuration(&configuration)) {
    return false;
  }

  *rf_region = configuration.rfRegion;
  return true;
}

bool SaveApplicationTxPowerlevel(zpal_tx_power_decidbm_t tx_power_level,
                                 zpal_tx_power_decidbm_t tx_power_adjust)
{
  SApplicationConfiguration configuration;

  if (!read_application_configuration(&configuration)) {
    return false;
  }

  configuration.iTxPower = tx_power_level;
  configuration.ipower0dbmMeasured = tx_power_adjust;
  return write_application_configuration(&configuration);
}

bool ReadApplicationTxPowerlevel(zpal_tx_power_decidbm_t *tx_power_level,
                                 zpal_tx_power_decidbm_t *tx_power_adjust)
{
  SApplicationConfiguration configuration;

  if (!read_application_configuration(&configuration)) {
    return false;
  }

  *tx_power_level = configuration.iTxPower;
  *tx_power_adjust = configuration.ipower0dbmMeasured;
  return true;
}

bool SaveApplicationMaxLRTxPwr(zpal_tx_power_decidbm_t max_tx_power_lr)
{
  SApplicationConfiguration configuration;

  if (!read_application_configuration(&configuration)) {
    return false;
  }

  configuration.maxTxPower = max_tx_power_lr;
  return write_application_configuration(&configuration);
}

bool ReadApplicationMaxLRTxPwr(zpal_tx_power_decidbm_t *max_tx_power_lr)
{
  SApplicationConfiguration configuration;

  if (!read_application_configuration(&configuration)) {
    return false;
  }

  *max_tx_power_lr = configuration.maxTxPower;
  return true;
}