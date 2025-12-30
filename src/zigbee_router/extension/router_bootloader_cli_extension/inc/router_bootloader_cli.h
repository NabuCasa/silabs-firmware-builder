/*
 * router_bootloader_cli.h
 *
 * CLI commands for bootloader info and reboot
 */

#ifndef ROUTER_BOOTLOADER_CLI_H_
#define ROUTER_BOOTLOADER_CLI_H_

#include "sl_cli.h"

void router_bootloader_cli_info(sl_cli_command_arg_t *arguments);
void router_bootloader_cli_reboot(sl_cli_command_arg_t *arguments);

#endif /* ROUTER_BOOTLOADER_CLI_H_ */
