/*
 * xncp_types.h
 *
 * Extensible NCP (XNCP) command framework types and definitions
 */

#ifndef XNCP_TYPES_H
#define XNCP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Feature flags - components OR these together
#define XNCP_FEATURE_MEMBER_OF_ALL_GROUPS  (1UL << 0)
#define XNCP_FEATURE_MANUAL_SOURCE_ROUTE   (1UL << 1)
#define XNCP_FEATURE_MFG_TOKEN_OVERRIDES   (1UL << 2)
#define XNCP_FEATURE_BUILD_STRING          (1UL << 3)
#define XNCP_FEATURE_FLOW_CONTROL_TYPE     (1UL << 4)
#define XNCP_FEATURE_CHIP_INFO             (1UL << 5)
#define XNCP_FEATURE_RESTORE_ROUTE_TABLE   (1UL << 6)
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

// Get aggregated feature flags from all handlers (generated)
uint32_t xncp_get_supported_features(void);

// Dispatch command to handlers (generated)
bool xncp_dispatch_command(xncp_context_t *ctx);

#endif // XNCP_TYPES_H
