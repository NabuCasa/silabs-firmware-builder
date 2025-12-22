/*
 * router_bootloader_cli.c
 *
 * CLI command handlers for bootloader info and reboot
 */

#include "router_bootloader_cli.h"
#include "btl_interface.h"
#include "sl_cli.h"
#include "app/framework/include/af.h"
#include <stdio.h>

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint16_t patch;
} gecko_version_t;

static gecko_version_t parse_version(uint32_t version)
{
   gecko_version_t result;

   result.major = (version >> 24) & 0xFF;
   result.minor = (version >> 16) & 0xFF;
   result.patch = version & 0xFFFF;

   return result;
}

void router_bootloader_cli_info(sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  char version_str[16];
  BootloaderInformation_t info = { .type = SL_BOOTLOADER, .version = 0L, .capabilities = 0L };

  bootloader_getInfo(&info);
  sl_zigbee_af_app_print("%s Bootloader", info.type == SL_BOOTLOADER ? "Gecko" : "Legacy" );

  gecko_version_t ver = parse_version(info.version);
  snprintf(version_str, sizeof(version_str),
          "v%u.%02u.%02u", ver.major, ver.minor, ver.patch);
  sl_zigbee_af_app_print(" %s\n", version_str);
}

void router_bootloader_cli_reboot(sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  bootloader_rebootAndInstall();
}
