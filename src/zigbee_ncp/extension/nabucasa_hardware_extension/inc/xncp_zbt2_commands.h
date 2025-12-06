/*
 * xncp_zbt2_commands.h
 *
 * ZBT-2 specific XNCP command definitions
 */

#ifndef XNCP_ZBT2_COMMANDS_H
#define XNCP_ZBT2_COMMANDS_H

#include "xncp_types.h"

bool xncp_zbt2_handle_command(xncp_context_t *ctx);
uint32_t xncp_zbt2_handle_command_features(void);

#endif // XNCP_ZBT2_COMMANDS_H
