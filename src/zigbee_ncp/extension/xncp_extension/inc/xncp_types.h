/*
 * xncp_types.h
 *
 * Extensible NCP (XNCP) command framework types and definitions
 */

#ifndef XNCP_TYPES_H
#define XNCP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// SDK compatibility layer
#ifdef STACK_TYPES_HEADER
// Simplicity SDK (2025.x)
#include "stack/include/sl_zigbee.h"
#include "stack/include/sl_zigbee_types_internal.h"
#include "stack/include/message.h"

typedef sl_status_t xncp_status_t;
#define XNCP_STATUS_OK           SL_STATUS_OK
#define XNCP_STATUS_BAD_ARGUMENT SL_STATUS_INVALID_PARAMETER
#define XNCP_STATUS_NOT_FOUND    SL_STATUS_NOT_FOUND

#define XNCP_MAX_SOURCE_ROUTE_RELAY_COUNT SL_ZIGBEE_MAX_SOURCE_ROUTE_RELAY_COUNT

typedef sl_zigbee_multicast_id_t xncp_multicast_id_t;
typedef sl_802154_short_addr_t xncp_node_id_t;
typedef sli_buffer_manager_buffer_t xncp_message_buffer_t;

#define xncp_get_pseudo_random_number sl_zigbee_get_pseudo_random_number
#define xncp_append_to_linked_buffers sl_legacy_buffer_manager_append_to_linked_buffers

#else
// Gecko SDK (4.x)
#include "ember.h"

typedef EmberStatus xncp_status_t;
#define XNCP_STATUS_OK           EMBER_SUCCESS
#define XNCP_STATUS_BAD_ARGUMENT EMBER_BAD_ARGUMENT
#define XNCP_STATUS_NOT_FOUND    EMBER_NOT_FOUND

#define XNCP_MAX_SOURCE_ROUTE_RELAY_COUNT EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT

typedef EmberMulticastId xncp_multicast_id_t;
typedef EmberNodeId xncp_node_id_t;
typedef EmberMessageBuffer xncp_message_buffer_t;

#define xncp_get_pseudo_random_number emberGetPseudoRandomNumber
#define xncp_append_to_linked_buffers emberAppendToLinkedBuffers

// EZSP enum aliases for new SDK names
#define SL_ZIGBEE_EZSP_MFG_STRING     EZSP_MFG_STRING
#define SL_ZIGBEE_EZSP_MFG_BOARD_NAME EZSP_MFG_BOARD_NAME

#endif

// Feature flags - components OR these together
#define XNCP_FEATURE_MEMBER_OF_ALL_GROUPS  (1UL << 0)
#define XNCP_FEATURE_MANUAL_SOURCE_ROUTE   (1UL << 1)
#define XNCP_FEATURE_MFG_TOKEN_OVERRIDES   (1UL << 2)
#define XNCP_FEATURE_BUILD_STRING          (1UL << 3)
#define XNCP_FEATURE_FLOW_CONTROL_TYPE     (1UL << 4)
#define XNCP_FEATURE_CHIP_INFO             (1UL << 5)
#define XNCP_FEATURE_RESTORE_ROUTE_TABLE   (1UL << 6)
#define XNCP_FEATURE_TX_POWER_INFO         (1UL << 7)
#define XNCP_FEATURE_COMBINED_SEND         (1UL << 8)
#define XNCP_FEATURE_LED_CONTROL           (1UL << 31)

// Base command IDs (extensions define their own in separate headers)
#define XNCP_CMD_GET_SUPPORTED_FEATURES_REQ 0x0000
#define XNCP_CMD_UNKNOWN                    0xFFFF

// Response bit - OR with request ID to get response ID
#define XNCP_CMD_RESPONSE_BIT 0x8000

// Command context passed to handlers
typedef struct {
    uint16_t command_id;
    uint8_t *payload;
    uint8_t payload_length;
    uint8_t *reply;
    uint8_t *reply_length;
    uint8_t *status;
    uint16_t *response_id;
} xncp_context_t;

// Handler function type - returns true if command was handled
typedef bool (*xncp_handler_fn_t)(xncp_context_t *ctx);

// Command definition for dispatch tables (sentinel-terminated with {0, NULL})
typedef struct {
    uint16_t command_id;
    xncp_handler_fn_t handler;
} xncp_command_def_t;

// Get aggregated feature flags from all handlers (generated)
uint32_t xncp_get_supported_features(void);

// Dispatch command to handlers (generated)
bool xncp_dispatch_command(xncp_context_t *ctx);

#endif // XNCP_TYPES_H
