#ifndef APPS_SERIALAPI_CMD_PROPRIETARY_H_
#define APPS_SERIALAPI_CMD_PROPRIETARY_H_

#include <stdint.h>
#include <stdbool.h>

#define FUNC_ID_NABU_CASA FUNC_ID_PROPRIETARY_0

/* FUNC_ID_PROPRIETARY_0 (Nabu Casa) command definitions */
typedef enum
{
  NABU_CASA_CMD_SUPPORTED = 0,
  NABU_CASA_LED_GET = 1,
  NABU_CASA_LED_SET = 2,
  /* NABU_CASA_GYRO_MEASURE (3) obsoleted */
  NABU_CASA_SYSTEM_INDICATION_SET = 4,
  NABU_CASA_CONFIG_GET = 5,
  NABU_CASA_CONFIG_SET = 6,
  NABU_CASA_LED_GET_BINARY = 7,
  NABU_CASA_LED_SET_BINARY = 8,
} eNabuCasaCmd;

typedef enum
{
  NC_SYS_INDICATION_OFF = 0,
  NC_SYS_INDICATION_WARN = 1,
  NC_SYS_INDICATION_ERROR = 2,
} eNabuCasaSystemIndication;

/* NVM file IDs for proprietary data */
#define FILE_ID_NABUCASA_LED    0x4660
#define FILE_ID_NABUCASA_CONFIG 0x4661

typedef struct __attribute__((packed)) NabuCasaLedStorage
{
  bool valid;
  uint8_t r;
  uint8_t g;
  uint8_t b;
} NabuCasaLedStorage_t;

typedef enum
{
  NC_CFG_ENABLE_TILT_INDICATOR = 0,
} eNabuCasaConfigKey;

typedef enum
{
  NC_CFG_FLAG_ENABLE_TILT_INDICATOR = (1 << NC_CFG_ENABLE_TILT_INDICATOR),
} eNabuCasaConfigFlags;

typedef struct __attribute__((packed)) NabuCasaConfigStorage
{
  eNabuCasaConfigFlags flags;
} NabuCasaConfigStorage_t;

#define CONFIG_STORAGE_DEFAULTS { \
  .flags = NC_CFG_FLAG_ENABLE_TILT_INDICATOR, \
}

bool nc_config_get(eNabuCasaConfigKey key);
void nc_config_set(eNabuCasaConfigKey key, bool value);

#endif /* APPS_SERIALAPI_CMD_PROPRIETARY_H_ */
