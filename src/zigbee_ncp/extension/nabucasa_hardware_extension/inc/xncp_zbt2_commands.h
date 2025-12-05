/*
 * xncp_zbt2_commands.h
 *
 * ZBT-2 specific XNCP command definitions
 */

#ifndef XNCP_ZBT2_COMMANDS_H
#define XNCP_ZBT2_COMMANDS_H

#include "xncp_types.h"

// ZBT-2 specific command IDs (0x0F00 range)
#define XNCP_CMD_SET_LED_STATE_REQ     0x0F00
#define XNCP_CMD_GET_ACCELEROMETER_REQ 0x0F01

// Handler and features functions
bool xncp_zbt2_handle_command(xncp_context_t *ctx);
uint32_t xncp_zbt2_handle_command_features(void);

#endif // XNCP_ZBT2_COMMANDS_H
