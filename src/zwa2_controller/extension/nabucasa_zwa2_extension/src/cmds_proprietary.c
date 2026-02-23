/**
 * @file cmds_proprietary.c
 * @brief Nabu Casa proprietary Serial API commands for ZWA-2
 *
 * Ported from sisdk-2024.12.1 to led_manager API (priority-based,
 * sleeptimer-driven). GYRO_MEASURE command removed (never used by host).
 */

#include <app.h>
#include <cmds_proprietary.h>
#include <string.h>
#include <ZAF_nvm_app.h>
#include "cmd_handlers.h"
#include "SerialAPI.h"
#include "led_manager_zwa2.h"
#include "led_manager_colors_zwa2.h"
#include "led_effects_zwa2.h"

#define BYTE_INDEX(x) (x / 8)
#define BYTE_OFFSET(x) (1 << (x % 8))
#define BITMASK_ADD_CMD(bitmask, cmd) (bitmask[BYTE_INDEX(cmd)] |= BYTE_OFFSET(cmd))

/* Track whether the manual LED layer is "on" (for GET commands) */
static bool manual_led_on = false;

bool nc_config_get(eNabuCasaConfigKey key)
{
  NabuCasaConfigStorage_t cfg = CONFIG_STORAGE_DEFAULTS;
  ZAF_nvm_app_read(FILE_ID_NABUCASA_CONFIG, &cfg, sizeof(cfg));
  return (cfg.flags & (1 << key)) != 0;
}

void nc_config_set(eNabuCasaConfigKey key, bool value)
{
  NabuCasaConfigStorage_t cfg = CONFIG_STORAGE_DEFAULTS;
  ZAF_nvm_app_read(FILE_ID_NABUCASA_CONFIG, &cfg, sizeof(cfg));

  if (value)
  {
    cfg.flags |= (1 << key);
  }
  else
  {
    cfg.flags &= ~(1 << key);
  }

  ZAF_nvm_app_write(FILE_ID_NABUCASA_CONFIG, &cfg, sizeof(cfg));
}

ZW_ADD_CMD(FUNC_ID_NABU_CASA)
{
  uint8_t inputLength = frame->len;
  const uint8_t *pInputBuffer = frame->payload;
  uint8_t response[BUF_SIZE_TX];
  uint8_t i = 0;
  uint8_t cmdRes;

  /* We assume operation is non-successful */
  cmdRes = false;

  if (1 > inputLength)
  {
    /* Command length must be at least 1 byte. Return with negative response in the out buffer */
    response[i++] = cmdRes;
    Respond(frame->cmd, response, i);
    return;
  }

  response[i++] = pInputBuffer[0]; /* Set output command ID equal input command ID */
  switch (pInputBuffer[0])
  {

  /* Report which subcommands are supported beside the NABU_CASA_CMD_SUPPORTED */
  case NABU_CASA_CMD_SUPPORTED:
    // HOST->ZW: NABU_CASA_CMD_SUPPORTED
    // ZW->HOST: NABU_CASA_CMD_SUPPORTED | supportedBitmask

    /* Report all supported commands as bitmask of their values */
    uint8_t supportedBitmask[32];
    memset(supportedBitmask, 0, sizeof(supportedBitmask));
    // Mark each command as supported
    BITMASK_ADD_CMD(supportedBitmask, NABU_CASA_CMD_SUPPORTED);
    BITMASK_ADD_CMD(supportedBitmask, NABU_CASA_LED_GET);
    BITMASK_ADD_CMD(supportedBitmask, NABU_CASA_LED_SET);
    BITMASK_ADD_CMD(supportedBitmask, NABU_CASA_LED_GET_BINARY);
    BITMASK_ADD_CMD(supportedBitmask, NABU_CASA_LED_SET_BINARY);
    BITMASK_ADD_CMD(supportedBitmask, NABU_CASA_SYSTEM_INDICATION_SET);
    BITMASK_ADD_CMD(supportedBitmask, NABU_CASA_CONFIG_GET);
    BITMASK_ADD_CMD(supportedBitmask, NABU_CASA_CONFIG_SET);

    // Copy as few bytes as necessary into the output buffer
    for (int j = 0; j <= NABU_CASA_LED_SET_BINARY / 8; j++)
    {
      response[i++] = supportedBitmask[j];
    }
    break;

  case NABU_CASA_LED_GET:
  {
    // HOST->ZW: NABU_CASA_LED_GET
    // ZW->HOST: NABU_CASA_LED_GET | r | g | b |

    // Get the current state of the LED
    rgb_t color = manual_led_on ? LED_COLOR_COLD_WHITE : LED_COLOR_BLACK;

    response[i++] = (uint8_t)(color.r >> 8);
    response[i++] = (uint8_t)(color.g >> 8);
    response[i++] = (uint8_t)(color.b >> 8);
    break;
  }

  case NABU_CASA_LED_SET:
    // HOST->ZW: NABU_CASA_LED_SET | r | g | b
    // ZW->HOST: NABU_CASA_LED_SET | true

    if (inputLength >= 4)
    {
      int pos = 1;
      uint8_t r = pInputBuffer[pos++];
      uint8_t g = pInputBuffer[pos++];
      uint8_t b = pInputBuffer[pos++];

      // Ignore actual color values, use cold white or off
      bool state = (r > 0 || g > 0 || b > 0);
      rgb_t color = state ? LED_COLOR_COLD_WHITE : LED_COLOR_BLACK;
      led_manager_set_color(LED_PRIORITY_MANUAL, color);
      manual_led_on = state;

      // Store the current color in NVM, so it can be restored after a reboot
      // FIXME: Only override if something changed
      NabuCasaLedStorage_t ledStorage = (NabuCasaLedStorage_t) {
        .valid = true,
        .r = (uint8_t)(color.r >> 8),
        .g = (uint8_t)(color.g >> 8),
        .b = (uint8_t)(color.b >> 8)
      };
      ZAF_nvm_app_write(FILE_ID_NABUCASA_LED, &ledStorage, sizeof(ledStorage));

      cmdRes = true;
    }
    response[i++] = cmdRes;
    break;

  case NABU_CASA_LED_GET_BINARY:
  {
    // HOST->ZW: NABU_CASA_LED_GET_BINARY
    // ZW->HOST: NABU_CASA_LED_GET_BINARY | state |

    // Get the current state of the LED
    if (manual_led_on) {
      response[i++] = true; // LED is on
    } else {
      response[i++] = false; // LED is off
    }
    break;
  }

  case NABU_CASA_LED_SET_BINARY:
  {
    // HOST->ZW: NABU_CASA_LED_SET_BINARY | state
    // ZW->HOST: NABU_CASA_LED_SET_BINARY | true

    if (inputLength >= 2)
    {
      // Set solid color as the LED effect
      bool state = pInputBuffer[1] != 0;
      rgb_t color = state ? LED_COLOR_COLD_WHITE : LED_COLOR_BLACK;
      led_manager_set_color(LED_PRIORITY_MANUAL, color);
      manual_led_on = state;

      // Store the current color in NVM, so it can be restored after a reboot
      // FIXME: Only override if something changed
      NabuCasaLedStorage_t ledStorage = (NabuCasaLedStorage_t) {
        .valid = true,
        .r = (uint8_t)(color.r >> 8),
        .g = (uint8_t)(color.g >> 8),
        .b = (uint8_t)(color.b >> 8)
      };
      ZAF_nvm_app_write(FILE_ID_NABUCASA_LED, &ledStorage, sizeof(ledStorage));

      cmdRes = true;
    }
    response[i++] = cmdRes;
    break;
  }

  case NABU_CASA_SYSTEM_INDICATION_SET:
    // HOST->ZW (REQ): NABU_CASA_SYSTEM_INDICATION_SET | severity
    // ZW->HOST (RES): NABU_CASA_SYSTEM_INDICATION_SET | true

    if (inputLength >= 2)
    {
      eNabuCasaSystemIndication severity = (eNabuCasaSystemIndication)pInputBuffer[1];
      switch (severity)
      {
      case NC_SYS_INDICATION_OFF:
        led_manager_clear_pattern(LED_PRIORITY_SYSTEM);
        cmdRes = true;
        break;
      case NC_SYS_INDICATION_WARN:
      {
        led_pattern_t warn_pattern = {
          .mode = LED_MODE_STATIC,
          .color = LED_COLOR_YELLOW,
          .duration_ms = 0
        };
        led_manager_set_pattern(LED_PRIORITY_SYSTEM, &warn_pattern);
        cmdRes = true;
        break;
      }
      case NC_SYS_INDICATION_ERROR:
      {
        led_pattern_t err_pattern = {
          .mode = LED_MODE_STATIC,
          .color = LED_COLOR_RED,
          .duration_ms = 0
        };
        led_manager_set_pattern(LED_PRIORITY_SYSTEM, &err_pattern);
        cmdRes = true;
        break;
      }
      default:
        // Unsupported. Do nothing
        break;
      }
    }

    response[i++] = cmdRes;
    break;

  case NABU_CASA_CONFIG_GET:
    // HOST->ZW (REQ): NABU_CASA_CONFIG_GET | key
    // ZW->HOST (RES): NABU_CASA_CONFIG_GET | key | size | value

    if (inputLength >= 2)
    {
      eNabuCasaConfigKey key = (eNabuCasaConfigKey)pInputBuffer[1];
      bool value = nc_config_get(key);
      response[i++] = key;
      response[i++] = 1;
      response[i++] = value;
    }
    else
    {
      // invalid command
      response[i++] = 0xFF;
    }
    break;

  case NABU_CASA_CONFIG_SET:
    // HOST->ZW (REQ): NABU_CASA_CONFIG_SET | key | size | value
    // ZW->HOST (RES): NABU_CASA_CONFIG_SET | success

    if (inputLength >= 4)
    {
      eNabuCasaConfigKey key = (eNabuCasaConfigKey)pInputBuffer[1];
      uint8_t size = pInputBuffer[2];
      if (size > 0 && size <= 4 && inputLength >= 3 + size)
      {
        int32_t value = 0;
        for (int j = 0; j < size; j++)
        {
          value |= (pInputBuffer[3 + j] << (j * 8));
        }

        switch (key)
        {
        case NC_CFG_ENABLE_TILT_INDICATOR:
        {
          // Save change in NVM
          bool enable = (value != 0);
          nc_config_set(key, enable);
          // and forward to application
          led_effects_set_tilt_enabled(enable);

          cmdRes = true;
          break;
        }
        default:
          // Unsupported. Do nothing
          break;
        }
      }
    }

    response[i++] = cmdRes;
    break;

  default:
    // Unsupported. Return false
    response[i++] = false;
    break;
  }

  Respond(frame->cmd, response, i);
}
