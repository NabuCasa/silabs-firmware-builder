/*
 * xncp_common_commands.c
 *
 * Common XNCP command handlers for all coordinators
 */

#include "xncp_common_commands.h"
#include "xncp_types.h"
#include "ember.h"
#include "ezsp-enum.h"
#include "em_usart.h"
#include "xncp_config.h"
#include "sl_iostream_usart_vcom_config.h"
#include <string.h>

// Flow control type enum
typedef enum {
  XNCP_FLOW_CONTROL_TYPE_SOFTWARE = 0x00,
  XNCP_FLOW_CONTROL_TYPE_HARDWARE = 0x01,
} XncpFlowControlType;

// External command handlers from xncp_source_route.c
extern bool xncp_set_source_route(xncp_context_t *ctx);
extern bool xncp_set_route_table_entry(xncp_context_t *ctx);
extern bool xncp_get_route_table_entry(xncp_context_t *ctx);
extern void xncp_source_route_init(void);

// SDK init callback
void xncp_common_init(uint8_t init_level)
{
    (void)init_level;
    xncp_source_route_init();
}

// Override to accept all multicast group packets (XNCP_FEATURE_MEMBER_OF_ALL_GROUPS)
bool __wrap_sli_zigbee_am_multicast_member(EmberMulticastId multicastId)
{
    (void)multicastId;
    // Ignore all binding and multicast table logic, we want all group packets
    return true;
}

static bool handle_get_mfg_token_override(xncp_context_t *ctx)
{
    if (ctx->payload_length != 1) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        *ctx->response_id = XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ | XNCP_CMD_RESPONSE_BIT;
        return true;
    }

    uint8_t token_id = ctx->payload[0];
    char *override_value;

    switch (token_id) {
        case EZSP_MFG_STRING:
            override_value = XNCP_MFG_MANUF_NAME;
            break;

        case EZSP_MFG_BOARD_NAME:
            override_value = XNCP_MFG_BOARD_NAME;
            break;

        default:
            *ctx->status = EMBER_NOT_FOUND;
            *ctx->response_id = XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ | XNCP_CMD_RESPONSE_BIT;
            return true;
    }

    uint8_t value_length = strlen(override_value);
    memcpy(ctx->reply + *ctx->reply_length, override_value, value_length);
    *ctx->reply_length += value_length;

    *ctx->status = EMBER_SUCCESS;
    *ctx->response_id = XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ | XNCP_CMD_RESPONSE_BIT;
    return true;
}

static bool handle_get_build_string(xncp_context_t *ctx)
{
    uint8_t value_length = strlen(XNCP_BUILD_STRING);
    memcpy(ctx->reply + *ctx->reply_length, XNCP_BUILD_STRING, value_length);
    *ctx->reply_length += value_length;

    *ctx->status = EMBER_SUCCESS;
    *ctx->response_id = XNCP_CMD_GET_BUILD_STRING_REQ | XNCP_CMD_RESPONSE_BIT;
    return true;
}

static bool handle_get_flow_control_type(xncp_context_t *ctx)
{
    XncpFlowControlType flow_control_type;

    switch (XNCP_FLOW_CONTROL_TYPE) {
        case usartHwFlowControlCtsAndRts:
            flow_control_type = XNCP_FLOW_CONTROL_TYPE_HARDWARE;
            break;

        case usartHwFlowControlNone:
        default:
            flow_control_type = XNCP_FLOW_CONTROL_TYPE_SOFTWARE;
            break;
    }

    ctx->reply[(*ctx->reply_length)++] = (uint8_t)(flow_control_type & 0xFF);

    *ctx->status = EMBER_SUCCESS;
    *ctx->response_id = XNCP_CMD_GET_FLOW_CONTROL_TYPE_REQ | XNCP_CMD_RESPONSE_BIT;
    return true;
}

static bool handle_get_chip_info(xncp_context_t *ctx)
{
    // RAM size
    ctx->reply[(*ctx->reply_length)++] = (uint8_t)((RAM_MEM_SIZE >>  0) & 0xFF);
    ctx->reply[(*ctx->reply_length)++] = (uint8_t)((RAM_MEM_SIZE >>  8) & 0xFF);
    ctx->reply[(*ctx->reply_length)++] = (uint8_t)((RAM_MEM_SIZE >> 16) & 0xFF);
    ctx->reply[(*ctx->reply_length)++] = (uint8_t)((RAM_MEM_SIZE >> 24) & 0xFF);

    // Part number
    uint8_t value_length = strlen(PART_NUMBER);
    ctx->reply[(*ctx->reply_length)++] = value_length;

    memcpy(ctx->reply + *ctx->reply_length, PART_NUMBER, value_length);
    *ctx->reply_length += value_length;

    *ctx->status = EMBER_SUCCESS;
    *ctx->response_id = XNCP_CMD_GET_CHIP_INFO_REQ | XNCP_CMD_RESPONSE_BIT;
    return true;
}

// Main command handler - called from generated dispatcher
bool xncp_common_handle_command(xncp_context_t *ctx)
{
    switch (ctx->command_id) {
        case XNCP_CMD_SET_SOURCE_ROUTE_REQ:
            return xncp_set_source_route(ctx);

        case XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ:
            return handle_get_mfg_token_override(ctx);

        case XNCP_CMD_GET_BUILD_STRING_REQ:
            return handle_get_build_string(ctx);

        case XNCP_CMD_GET_FLOW_CONTROL_TYPE_REQ:
            return handle_get_flow_control_type(ctx);

        case XNCP_CMD_GET_CHIP_INFO_REQ:
            return handle_get_chip_info(ctx);

        case XNCP_CMD_SET_ROUTE_TABLE_ENTRY_REQ:
            return xncp_set_route_table_entry(ctx);

        case XNCP_CMD_GET_ROUTE_TABLE_ENTRY_REQ:
            return xncp_get_route_table_entry(ctx);

        default:
            return false;  // Not handled
    }
}

// Return feature flags for common commands
uint32_t xncp_common_handle_command_features(void)
{
    return XNCP_FEATURE_MEMBER_OF_ALL_GROUPS
         | XNCP_FEATURE_MANUAL_SOURCE_ROUTE
         | XNCP_FEATURE_MFG_TOKEN_OVERRIDES
         | XNCP_FEATURE_BUILD_STRING
         | XNCP_FEATURE_FLOW_CONTROL_TYPE
         | XNCP_FEATURE_CHIP_INFO
         | XNCP_FEATURE_RESTORE_ROUTE_TABLE;
}
