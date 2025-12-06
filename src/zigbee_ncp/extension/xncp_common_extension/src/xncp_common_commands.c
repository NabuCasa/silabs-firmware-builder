/*
 * xncp_common_commands.c
 *
 * Common XNCP command handlers for all coordinators
 */

#include "xncp_common_commands.h"
#include "xncp_types.h"
#include "xncp_config.h"
#include "ember.h"
#include "ezsp-enum.h"
#include "em_usart.h"
#include "random.h"
#include "sl_iostream_usart_vcom_config.h"
#include <string.h>

#define BUILD_UINT16(low, high) (((uint16_t)(low)) | ((uint16_t)(high) << 8))

typedef enum {
  XNCP_FLOW_CONTROL_TYPE_SOFTWARE = 0x00,
  XNCP_FLOW_CONTROL_TYPE_HARDWARE = 0x01,
} XncpFlowControlType;

typedef enum {
  EMBER_ROUTE_ACTIVE = 0,
  EMBER_ROUTE_BEING_DISCOVERED = 1,
  EMBER_ROUTE_UNUSED = 3,
  EMBER_ROUTE_VALIDATING = 4
} EmberRouteStatus;

typedef struct ManualSourceRoute {
  bool active;
  uint16_t destination;
  uint8_t num_relays;
  uint16_t relays[EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT];
} ManualSourceRoute;

static ManualSourceRoute manual_source_routes[XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE];

// External references to EmberZNet route table
extern sli_zigbee_route_table_entry_t sli_zigbee_route_table[];
extern uint8_t sli_zigbee_route_table_size;

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------

static bool handle_set_source_route(xncp_context_t *ctx);
static bool handle_get_mfg_token_override(xncp_context_t *ctx);
static bool handle_get_build_string(xncp_context_t *ctx);
static bool handle_get_flow_control_type(xncp_context_t *ctx);
static bool handle_get_chip_info(xncp_context_t *ctx);
static bool handle_set_route_table_entry(xncp_context_t *ctx);
static bool handle_get_route_table_entry(xncp_context_t *ctx);

//------------------------------------------------------------------------------
// Command table
//------------------------------------------------------------------------------

const xncp_command_def_t xncp_common_commands[] = {
    {0x0001, handle_set_source_route},
    {0x0002, handle_get_mfg_token_override},
    {0x0003, handle_get_build_string},
    {0x0004, handle_get_flow_control_type},
    {0x0005, handle_get_chip_info},
    {0x0006, handle_set_route_table_entry},
    {0x0007, handle_get_route_table_entry},
    {0, NULL}  // sentinel
};

//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------

// SDK init callback
void xncp_common_init(uint8_t init_level)
{
    (void)init_level;
    for (uint8_t i = 0; i < XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE; i++) {
        manual_source_routes[i].active = false;
    }
}

//------------------------------------------------------------------------------
// Multicast override (XNCP_FEATURE_MEMBER_OF_ALL_GROUPS)
//------------------------------------------------------------------------------

bool __wrap_sli_zigbee_am_multicast_member(EmberMulticastId multicastId)
{
    (void)multicastId;
    // Ignore all binding and multicast table logic, we want all group packets
    return true;
}

//------------------------------------------------------------------------------
// Source route management (XNCP_FEATURE_MANUAL_SOURCE_ROUTE)
//------------------------------------------------------------------------------

static ManualSourceRoute* get_manual_source_route(EmberNodeId destination)
{
    for (uint8_t i = 0; i < XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE; i++) {
        if (manual_source_routes[i].active &&
            manual_source_routes[i].destination == destination) {
            return &manual_source_routes[i];
        }
    }
    return NULL;
}

static sli_zigbee_route_table_entry_t* find_free_routing_table_entry(EmberNodeId destination)
{
    uint8_t index = 0xFF;
    sli_zigbee_route_table_entry_t *route_table_entry = NULL;

    for (uint8_t i = 0; i < sli_zigbee_route_table_size; i++) {
        route_table_entry = &sli_zigbee_route_table[i];

        if (route_table_entry->destination == destination) {
            return route_table_entry;
        } else if (route_table_entry->status == EMBER_ROUTE_UNUSED) {
            index = i;
            continue;
        }
    }

    if (index == 0xFF) {
        index = emberGetPseudoRandomNumber() % sli_zigbee_route_table_size;
    }

    return &sli_zigbee_route_table[index];
}

// Callback registered via template_contribution
void nc_zigbee_override_append_source_route(uint16_t destination,
                                             void *header,
                                             bool *consumed)
{
    EmberMessageBuffer *msg_header = (EmberMessageBuffer *)header;
    ManualSourceRoute *route = get_manual_source_route(destination);

    if (route == NULL) {
        *consumed = false;
        return;
    }

    *consumed = true;

    // Empty source routes are invalid according to the spec
    if (route->num_relays == 0) {
        // Add a fake route to the routing table to help EmberZNet just send the packet
        sli_zigbee_route_table_entry_t *route_table_entry = find_free_routing_table_entry(route->destination);
        route_table_entry->destination = route->destination;
        route_table_entry->nextHop = route->destination;
        route_table_entry->status = EMBER_ROUTE_ACTIVE;
        route_table_entry->cost = 0;
        route_table_entry->networkIndex = 0;
        return;
    }

    uint8_t relay_index = route->num_relays - 1;

    route->active = false;  // Disable the route after a single use

    emberAppendToLinkedBuffers(*msg_header, &route->num_relays, 1);
    emberAppendToLinkedBuffers(*msg_header, &relay_index, 1);

    for (uint8_t i = 0; i < route->num_relays; i++) {
        emberAppendToLinkedBuffers(*msg_header, (uint8_t*)&route->relays[route->num_relays - i - 1], 2);
    }
}

static bool handle_set_source_route(xncp_context_t *ctx)
{
    if ((ctx->payload_length < 2) || (ctx->payload_length % 2 != 0)) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        return true;
    }

    uint8_t num_relays = (ctx->payload_length - 2) / 2;

    if (num_relays > EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT + 1) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        return true;
    }

    uint8_t insertion_index = emberGetPseudoRandomNumber() % XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE;
    uint16_t node_id = BUILD_UINT16(ctx->payload[0], ctx->payload[1]);

    for (uint8_t i = 0; i < XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE; i++) {
        ManualSourceRoute *route = &manual_source_routes[i];

        if (route->active == false) {
            insertion_index = i;
        } else if (route->destination == node_id) {
            insertion_index = i;
            break;
        }
    }

    ManualSourceRoute *route = &manual_source_routes[insertion_index];

    for (uint8_t i = 0; i < num_relays; i++) {
        uint16_t relay = BUILD_UINT16(ctx->payload[2 + 2 * i + 0], ctx->payload[2 + 2 * i + 1]);
        route->relays[i] = relay;
    }

    route->destination = node_id;
    route->num_relays = num_relays;
    route->active = true;

    *ctx->status = EMBER_SUCCESS;
    return true;
}

//------------------------------------------------------------------------------
// Route table management (XNCP_FEATURE_RESTORE_ROUTE_TABLE)
//------------------------------------------------------------------------------

static bool handle_set_route_table_entry(xncp_context_t *ctx)
{
    if (ctx->payload_length != 7) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        return true;
    }

    uint8_t route_table_index = ctx->payload[0];
    if (route_table_index >= sli_zigbee_route_table_size) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        return true;
    }

    sli_zigbee_route_table_entry_t *entry = &sli_zigbee_route_table[route_table_index];
    entry->destination = BUILD_UINT16(ctx->payload[1], ctx->payload[2]);
    entry->nextHop = BUILD_UINT16(ctx->payload[3], ctx->payload[4]);
    entry->status = ctx->payload[5];
    entry->cost = ctx->payload[6];
    entry->networkIndex = 0;

    *ctx->status = EMBER_SUCCESS;
    return true;
}

static bool handle_get_route_table_entry(xncp_context_t *ctx)
{
    if (ctx->payload_length != 1) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        return true;
    }

    uint8_t route_table_index = ctx->payload[0];
    if (route_table_index >= sli_zigbee_route_table_size) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        return true;
    }

    sli_zigbee_route_table_entry_t *entry = &sli_zigbee_route_table[route_table_index];

    ctx->reply[(*ctx->reply_length)++] = (entry->destination & 0x00FF) >> 0;
    ctx->reply[(*ctx->reply_length)++] = (entry->destination & 0xFF00) >> 8;

    ctx->reply[(*ctx->reply_length)++] = (entry->nextHop & 0x00FF) >> 0;
    ctx->reply[(*ctx->reply_length)++] = (entry->nextHop & 0xFF00) >> 8;

    ctx->reply[(*ctx->reply_length)++] = entry->status;
    ctx->reply[(*ctx->reply_length)++] = entry->cost;

    *ctx->status = EMBER_SUCCESS;
    return true;
}

//------------------------------------------------------------------------------
// Token and info commands
//------------------------------------------------------------------------------

static bool handle_get_mfg_token_override(xncp_context_t *ctx)
{
    if (ctx->payload_length != 1) {
        *ctx->status = EMBER_BAD_ARGUMENT;
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
            return true;
    }

    uint8_t value_length = strlen(override_value);
    memcpy(ctx->reply + *ctx->reply_length, override_value, value_length);
    *ctx->reply_length += value_length;

    *ctx->status = EMBER_SUCCESS;
    return true;
}

static bool handle_get_build_string(xncp_context_t *ctx)
{
    uint8_t value_length = strlen(XNCP_BUILD_STRING);
    memcpy(ctx->reply + *ctx->reply_length, XNCP_BUILD_STRING, value_length);
    *ctx->reply_length += value_length;

    *ctx->status = EMBER_SUCCESS;
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
    return true;
}
