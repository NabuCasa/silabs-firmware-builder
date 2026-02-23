/*
 * xncp_core.c
 *
 * Extensible NCP (XNCP) command framework core implementation
 */

#include "xncp_types.h"
#include "xncp_dispatcher.h"
#include "ember.h"

#define BUILD_UINT16(low, high) (((uint16_t)(low)) | ((uint16_t)(high) << 8))

EmberStatus emberAfPluginXncpIncomingCustomFrameCallback(
    uint8_t messageLength,
    uint8_t *messagePayload,
    uint8_t *replyPayloadLength,
    uint8_t *replyPayload)
{
    uint8_t rsp_status = EMBER_SUCCESS;
    uint16_t rsp_command_id = XNCP_CMD_UNKNOWN;

    if (messageLength < 3) {
        rsp_status = EMBER_BAD_ARGUMENT;
        goto respond;
    }

    uint16_t req_command_id = BUILD_UINT16(messagePayload[0], messagePayload[1]);
    // uint8_t req_status = messagePayload[2];  // Unused

    // Strip the packet header to simplify command parsing
    messagePayload += 3;
    messageLength -= 3;

    // Leave space for the reply packet header
    *replyPayloadLength = 3;

    // Handle get_supported_features internally
    if (req_command_id == XNCP_CMD_GET_SUPPORTED_FEATURES_REQ) {
        uint32_t features = xncp_get_supported_features();
        rsp_command_id = XNCP_CMD_GET_SUPPORTED_FEATURES_REQ | XNCP_CMD_RESPONSE_BIT;
        replyPayload[(*replyPayloadLength)++] = (uint8_t)((features >>  0) & 0xFF);
        replyPayload[(*replyPayloadLength)++] = (uint8_t)((features >>  8) & 0xFF);
        replyPayload[(*replyPayloadLength)++] = (uint8_t)((features >> 16) & 0xFF);
        replyPayload[(*replyPayloadLength)++] = (uint8_t)((features >> 24) & 0xFF);
        goto respond;
    }

    // Try dispatching to registered handlers
    xncp_context_t ctx = {
        .command_id = req_command_id,
        .payload = messagePayload,
        .payload_length = messageLength,
        .reply = replyPayload,
        .reply_length = replyPayloadLength,
        .status = &rsp_status,
        .response_id = &rsp_command_id
    };

    if (!xncp_dispatch_command(&ctx)) {
        rsp_status = EMBER_NOT_FOUND;
    }

respond:
    replyPayload[0] = (uint8_t)((rsp_command_id >> 0) & 0xFF);
    replyPayload[1] = (uint8_t)((rsp_command_id >> 8) & 0xFF);
    replyPayload[2] = rsp_status;
    return EMBER_SUCCESS;
}
