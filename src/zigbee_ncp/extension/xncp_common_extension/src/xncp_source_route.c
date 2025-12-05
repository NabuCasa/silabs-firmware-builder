/*
 * xncp_source_route.c
 *
 * Manual source route table management for XNCP
 */

#include "xncp_common_commands.h"
#include "xncp_types.h"
#include "ember.h"
#include "random.h"
#include "xncp_config.h"

// Route table entry status values
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

void xncp_source_route_init(void)
{
    for (uint8_t i = 0; i < XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE; i++) {
        manual_source_routes[i].active = false;
    }
}

ManualSourceRoute* xncp_get_manual_source_route(EmberNodeId destination)
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
            // Prefer a route to the destination, if one exists
            return route_table_entry;
        } else if (route_table_entry->status == EMBER_ROUTE_UNUSED) {
            // Otherwise, keep track of the most recent unused route
            index = i;
            continue;
        }
    }

    // If we have no free route slots, pick one at random to replace
    if (index == 0xFF) {
        index = emberGetPseudoRandomNumber() % sli_zigbee_route_table_size;
    }

    return &sli_zigbee_route_table[index];
}

bool xncp_set_source_route(xncp_context_t *ctx)
{
    #define BUILD_UINT16(low, high) (((uint16_t)(low)) | ((uint16_t)(high) << 8))

    if ((ctx->payload_length < 2) || (ctx->payload_length % 2 != 0)) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        *ctx->response_id = XNCP_CMD_SET_SOURCE_ROUTE_REQ | XNCP_CMD_RESPONSE_BIT;
        return true;
    }

    uint8_t num_relays = (ctx->payload_length - 2) / 2;

    if (num_relays > EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT + 1) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        *ctx->response_id = XNCP_CMD_SET_SOURCE_ROUTE_REQ | XNCP_CMD_RESPONSE_BIT;
        return true;
    }

    // If we don't find a better index, pick one at random to replace
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
    *ctx->response_id = XNCP_CMD_SET_SOURCE_ROUTE_REQ | XNCP_CMD_RESPONSE_BIT;
    return true;

    #undef BUILD_UINT16
}

// Callback registered via template_contribution
void nc_zigbee_override_append_source_route(uint16_t destination,
                                             void *header,
                                             bool *consumed)
{
    EmberMessageBuffer *msg_header = (EmberMessageBuffer *)header;
    ManualSourceRoute *route = xncp_get_manual_source_route(destination);

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

bool xncp_set_route_table_entry(xncp_context_t *ctx)
{
    #define BUILD_UINT16(low, high) (((uint16_t)(low)) | ((uint16_t)(high) << 8))

    if (ctx->payload_length != 7) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        *ctx->response_id = XNCP_CMD_SET_ROUTE_TABLE_ENTRY_REQ | XNCP_CMD_RESPONSE_BIT;
        return true;
    }

    uint8_t route_table_index = ctx->payload[0];
    if (route_table_index >= sli_zigbee_route_table_size) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        *ctx->response_id = XNCP_CMD_SET_ROUTE_TABLE_ENTRY_REQ | XNCP_CMD_RESPONSE_BIT;
        return true;
    }

    sli_zigbee_route_table_entry_t *entry = &sli_zigbee_route_table[route_table_index];
    entry->destination = BUILD_UINT16(ctx->payload[1], ctx->payload[2]);
    entry->nextHop = BUILD_UINT16(ctx->payload[3], ctx->payload[4]);
    entry->status = ctx->payload[5];
    entry->cost = ctx->payload[6];
    entry->networkIndex = 0;

    *ctx->status = EMBER_SUCCESS;
    *ctx->response_id = XNCP_CMD_SET_ROUTE_TABLE_ENTRY_REQ | XNCP_CMD_RESPONSE_BIT;
    return true;

    #undef BUILD_UINT16
}

bool xncp_get_route_table_entry(xncp_context_t *ctx)
{
    if (ctx->payload_length != 1) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        *ctx->response_id = XNCP_CMD_GET_ROUTE_TABLE_ENTRY_REQ | XNCP_CMD_RESPONSE_BIT;
        return true;
    }

    uint8_t route_table_index = ctx->payload[0];
    if (route_table_index >= sli_zigbee_route_table_size) {
        *ctx->status = EMBER_BAD_ARGUMENT;
        *ctx->response_id = XNCP_CMD_GET_ROUTE_TABLE_ENTRY_REQ | XNCP_CMD_RESPONSE_BIT;
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
    *ctx->response_id = XNCP_CMD_GET_ROUTE_TABLE_ENTRY_REQ | XNCP_CMD_RESPONSE_BIT;
    return true;
}
