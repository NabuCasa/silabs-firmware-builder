/*
 * xncp_common_commands.h
 *
 * Common XNCP command definitions for all coordinators
 */

#ifndef XNCP_COMMON_COMMANDS_H
#define XNCP_COMMON_COMMANDS_H

#include "xncp_types.h"

bool xncp_common_handle_command(xncp_context_t *ctx);
uint32_t xncp_common_handle_command_features(void);

#endif // XNCP_COMMON_COMMANDS_H
