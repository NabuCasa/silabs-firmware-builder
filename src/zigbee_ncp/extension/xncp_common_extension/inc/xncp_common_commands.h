/*
 * xncp_common_commands.h
 *
 * Common XNCP command definitions for all coordinators
 */

#ifndef XNCP_COMMON_COMMANDS_H
#define XNCP_COMMON_COMMANDS_H

#include "xncp_types.h"

// Common command IDs (0x0001 - 0x00FF range)
#define XNCP_CMD_SET_SOURCE_ROUTE_REQ       0x0001
#define XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ 0x0002
#define XNCP_CMD_GET_BUILD_STRING_REQ       0x0003
#define XNCP_CMD_GET_FLOW_CONTROL_TYPE_REQ  0x0004
#define XNCP_CMD_GET_CHIP_INFO_REQ          0x0005
#define XNCP_CMD_SET_ROUTE_TABLE_ENTRY_REQ  0x0006
#define XNCP_CMD_GET_ROUTE_TABLE_ENTRY_REQ  0x0007

// Handler and features functions
bool xncp_common_handle_command(xncp_context_t *ctx);
uint32_t xncp_common_handle_command_features(void);

#endif // XNCP_COMMON_COMMANDS_H
