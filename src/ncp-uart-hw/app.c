/***************************************************************************//**
 * @file app.c
 * @brief Callbacks implementation and application specific code.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include PLATFORM_HEADER
#include "ember.h"
#include "ember-types.h"
#include "ezsp-enum.h"
#include "random.h"
#include "mac-flat-header.h"

#include "stack/include/message.h"

#include "config/xncp_config.h"


#define BUILD_UINT16(low, high)  (((uint16_t)(low) << 0) | ((uint16_t)(high) << 8))
#define emberPacketHandoffIncomingHandler sli_zigbee_af_packet_handoff_incoming_callback
#define emberPacketHandoffOutgoingHandler sli_zigbee_af_packet_handoff_outgoing_callback

extern sli_zigbee_route_table_entry_t sli_zigbee_route_table[];
extern uint8_t sli_zigbee_route_table_size;

typedef enum {
  XNCP_CMD_GET_SUPPORTED_FEATURES_REQ = 0x0000,
  XNCP_CMD_SET_SOURCE_ROUTE_REQ       = 0x0001,
  XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ = 0x0002,

  XNCP_CMD_GET_SUPPORTED_FEATURES_RSP = XNCP_CMD_GET_SUPPORTED_FEATURES_REQ | 0x8000,
  XNCP_CMD_SET_SOURCE_ROUTE_RSP       = XNCP_CMD_SET_SOURCE_ROUTE_REQ       | 0x8000,
  XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_RSP = XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ | 0x8000,

  XNCP_CMD_UNKNOWN = 0xFFFF
} XNCP_COMMAND;


#define FEATURE_MEMBER_OF_ALL_GROUPS  (0b00000000000000000000000000000001)
#define FEATURE_MANUAL_SOURCE_ROUTE   (0b00000000000000000000000000000010)
#define FEATURE_MFG_TOKEN_OVERRIDES   (0b00000000000000000000000000000100)
#define SUPPORTED_FEATURES ( \
      FEATURE_MEMBER_OF_ALL_GROUPS \
    | FEATURE_MANUAL_SOURCE_ROUTE \
    | FEATURE_MFG_TOKEN_OVERRIDES \
)


typedef struct ManualSourceRoute {
  bool active;
  uint16_t destination;
  uint8_t num_relays;
  uint16_t relays[EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT];
} ManualSourceRoute;

ManualSourceRoute manual_source_routes[XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE];


ManualSourceRoute* get_manual_source_route(EmberNodeId destination)
{
  uint8_t index = 0xFF;

  for (uint8_t i = 0; i < XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE; i++) {
    if (manual_source_routes[i].active && (manual_source_routes[i].destination == destination)) {
      index = i;
      break;
    }
  }

  if (index == 0xFF) {
    return NULL;
  }

  return &manual_source_routes[index];
}

//----------------------
// Implemented Callbacks

/** @brief
 *
 * Application framework equivalent of ::emberRadioNeedsCalibratingHandler
 */
void emberAfRadioNeedsCalibratingCallback(void)
{
  sl_mac_calibrate_current_channel();
}

/** @brief Init
 * Application init function
 */
void emberAfMainInitCallback(void)
{
  for (uint8_t i = 0; i < XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE; i++) {
    manual_source_routes[i].active = false;
  }
}

/** @brief Incoming packet filter callback
 *
 * Filters and/or mutates incoming packets. Currently used only for wildcard multicast
 * group membership.
 */
EmberPacketAction emberPacketHandoffIncomingHandler(EmberZigbeePacketType packetType,
                                                                 EmberMessageBuffer packetBuffer,
                                                                 uint8_t index,
                                                                 void *data)
{
  if (packetType != EMBER_ZIGBEE_PACKET_TYPE_APS_DATA) {
    return EMBER_ACCEPT_PACKET;
  }

  uint8_t* packetData = emberMessageBufferContents(packetBuffer);
  uint16_t packetSize = emberMessageBufferLength(packetBuffer);

  // Skip over the 802.15.4 header to the payload
  uint8_t payload_offset = sl_mac_flat_field_offset(packetData, true, EMBER_PH_FIELD_MAC_PAYLOAD);
  packetData += payload_offset;
  packetSize -= payload_offset;

  if (packetSize < 3) {
    return EMBER_ACCEPT_PACKET;
  }

  uint8_t deliveryMode = (packetData[0] & 0b00001100) >> 2;

  // Only look at multicast packets
  if (deliveryMode != 0x03) {
    return EMBER_ACCEPT_PACKET;
  }

  // Take ownership over the first entry and continuously rewrite it
  EmberMulticastTableEntry *tableEntry = &(sl_zigbee_get_multicast_table()[0]);

  tableEntry->endpoint = 1;
  tableEntry->multicastId = BUILD_UINT16(packetData[1], packetData[2]);
  tableEntry->networkIndex = 0;

  return EMBER_ACCEPT_PACKET;
}

#define emberPacketHandoffOutgoingHandler sli_zigbee_af_packet_handoff_outgoing_callback

/** @brief Outgoing packet filter callback
 *
 * Filters and/or mutates outgoing packets. Currently injects manual source routes.
 */
EmberPacketAction emberPacketHandoffOutgoingHandler(EmberZigbeePacketType packetType,
                                                                 EmberMessageBuffer packetBuffer,
                                                                 uint8_t index,
                                                                 void *data)
{
  // Only mutate NWK data packets
  if (packetType != EMBER_ZIGBEE_PACKET_TYPE_NWK_DATA) {
    return EMBER_ACCEPT_PACKET;
  }

  uint8_t* packetData = emberMessageBufferContents(packetBuffer);
  uint16_t packetSize = emberMessageBufferLength(packetBuffer);

  // Skip over the 802.15.4 header to the payload
  uint8_t payload_offset = sl_mac_flat_field_offset(packetData, true, EMBER_PH_FIELD_MAC_PAYLOAD);
  packetData += payload_offset;
  packetSize -= payload_offset;

  if (packetSize < 4) {
    return EMBER_ACCEPT_PACKET;
  }

  uint16_t frame_control = BUILD_UINT16(packetData[0], packetData[1]);

  // Don't mutate multicast traffic
  if (frame_control & 0b0000000100000000) {
    return EMBER_ACCEPT_PACKET;
  }

  uint16_t destination = BUILD_UINT16(packetData[2], packetData[3]);

  // Don't mutate broadcast traffic
  if (destination >= 0xFFFC) {
      return EMBER_ACCEPT_PACKET;
  }

  ManualSourceRoute* source_route = get_manual_source_route(destination);

  // If we do not have a source route, continue as normal
  if (source_route == NULL) {
    return EMBER_ACCEPT_PACKET;
  }

  // Flip the source routing bit
  packetData[1] |= 0b00000100;

  // And make room for the source route: relay_count, relay_index, list of relays
  uint8_t bytes_to_grow = 1 + 1 + 2 * source_route->num_relays;

  // Grow and re-load the buffer, ensuring we again skip ahead to the payload
  emberExtendLinkedBuffer(packetBuffer, bytes_to_grow);
  packetData = emberMessageBufferContents(packetBuffer);
  packetData += payload_offset;

  uint8_t aux_header_size = 2;

  // Extended source
  if (frame_control & 0b0001000000000000) {
    aux_header_size += 8;
  } else {
    aux_header_size += 2;
  }

  // Extended destination
  if (frame_control & 0b0000100000000000) {
    aux_header_size += 8;
  } else {
    aux_header_size += 2;
  }

  // Radius and sequence
  aux_header_size += 2;

  // Shift the packet data to make room between the AUX header and the plaintext payload
  for (uint8_t i = 0; i < packetSize - aux_header_size; i++) {
      packetData[aux_header_size + bytes_to_grow + i] = packetData[aux_header_size + i];
  }

  packetData[aux_header_size + 0] = source_route->num_relays;
  packetData[aux_header_size + 1] = 0;  // Relay index

  for (uint8_t i = 0; i < source_route->num_relays; i++) {
    packetData[aux_header_size + 2 + 2 * i + 0] = (source_route->relays[i] >> 0) & 0xFF;
    packetData[aux_header_size + 2 + 2 * i + 1] = (source_route->relays[i] >> 8) & 0xFF;
  }

  return EMBER_MANGLE_PACKET;
}


EmberStatus emberAfPluginXncpIncomingCustomFrameCallback(uint8_t messageLength,
                                                         uint8_t *messagePayload,
                                                         uint8_t *replyPayloadLength,
                                                         uint8_t *replyPayload) {
  uint8_t rsp_status = EMBER_SUCCESS;
  uint16_t rsp_command_id = XNCP_CMD_UNKNOWN;

  if (messageLength < 3) {
    rsp_status = EMBER_BAD_ARGUMENT;

    replyPayload[0] = (uint8_t)((rsp_command_id >> 0) & 0xFF);
    replyPayload[1] = (uint8_t)((rsp_command_id >> 8) & 0xFF);
    replyPayload[2] = rsp_status;
    return EMBER_SUCCESS;
  }

  uint16_t req_command_id = BUILD_UINT16(messagePayload[0], messagePayload[1]);
  uint8_t req_status = messagePayload[2];
  (void)req_status;

  // Strip the packet header to simplify command parsing below
  messagePayload += 3;
  messageLength -= 3;

  // Leave space for the reply packet header
  *replyPayloadLength = 0;
  *replyPayloadLength += 2;  // Space for the response command ID
  *replyPayloadLength += 1;  // Space for the status

  switch (req_command_id) {
    case XNCP_CMD_GET_SUPPORTED_FEATURES_REQ: {
      rsp_command_id = XNCP_CMD_GET_SUPPORTED_FEATURES_RSP;
      rsp_status = EMBER_SUCCESS;

      replyPayload[(*replyPayloadLength)++] = (uint8_t)((SUPPORTED_FEATURES >>  0) & 0xFF);
      replyPayload[(*replyPayloadLength)++] = (uint8_t)((SUPPORTED_FEATURES >>  8) & 0xFF);
      replyPayload[(*replyPayloadLength)++] = (uint8_t)((SUPPORTED_FEATURES >> 16) & 0xFF);
      replyPayload[(*replyPayloadLength)++] = (uint8_t)((SUPPORTED_FEATURES >> 24) & 0xFF);
      break;
    }

    case XNCP_CMD_SET_SOURCE_ROUTE_REQ: {
      rsp_command_id = XNCP_CMD_SET_SOURCE_ROUTE_RSP;
      rsp_status = EMBER_SUCCESS;

      if ((messageLength < 2) || (messageLength % 2 != 0)) {
        rsp_status = EMBER_BAD_ARGUMENT;
        break;
      }

      uint8_t num_relays = (messageLength - 2) / 2;

      if (num_relays > EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT + 1) {
        rsp_status = EMBER_BAD_ARGUMENT;
        break;
      }

      // If we don't find a better index, pick one at random to replace
      uint8_t insertion_index = emberGetPseudoRandomNumber() % XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE;
      uint16_t node_id = BUILD_UINT16(messagePayload[0], messagePayload[1]);

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
        uint16_t relay = BUILD_UINT16(messagePayload[2 + 2 * i + 0], messagePayload[2 + 2 * i + 1]);
        route->relays[i] = relay;
      }

      route->destination = node_id;
      route->num_relays = num_relays;
      route->active = true;

      // Add a fake route to the routing table to skip routing to the node
      sli_zigbee_route_table_entry_t *route_table_entry = &sli_zigbee_route_table[0];
      route_table_entry->destination = node_id;
      route_table_entry->nextHop = node_id;
      route_table_entry->status = 0;  // ACTIVE=0
      route_table_entry->cost = 0;
      route_table_entry->networkIndex = 0;

      break;
    }

    case XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ: {
      rsp_command_id = XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_RSP;
      rsp_status = EMBER_SUCCESS;

      if (messageLength != 1) {
        rsp_status = EMBER_BAD_ARGUMENT;
        break;
      }

      uint8_t token_id = messagePayload[0];
      char *override_value;

      switch (token_id) {
        case EZSP_MFG_STRING: {
          override_value = XNCP_MFG_MANUF_NAME;
          break;
        }

        case EZSP_MFG_BOARD_NAME: {
          override_value = XNCP_MFG_BOARD_NAME;
          break;
        }

        default: {
          rsp_status = EMBER_NOT_FOUND;
          override_value = "";
          break;
        }
      }

      uint8_t value_length = strlen(override_value);
      memcpy(replyPayload + *replyPayloadLength, override_value, value_length);
      *replyPayloadLength += value_length;
      break;
    }

    default: {
      rsp_status = EMBER_NOT_FOUND;
      break;
    }
  }

  replyPayload[0] = (uint8_t)((rsp_command_id >> 0) & 0xFF);
  replyPayload[1] = (uint8_t)((rsp_command_id >> 8) & 0xFF);
  replyPayload[2] = rsp_status;

  return EMBER_SUCCESS;
}
