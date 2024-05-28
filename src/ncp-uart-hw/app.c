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

#include "stack/include/message.h"


typedef enum {
  XNCP_CMD_GET_SUPPORTED_FEATURES = 0x0000,
  XNCP_CMD_SET_SOURCE_ROUTE = 0x0001
} CUSTOM_EZSP_CMD;


#define FEATURE_MEMBER_OF_ALL_GROUPS (0b00000000000000000000000000000001)
#define FEATURE_MANUAL_SOURCE_ROUTE  (0b00000000000000000000000000000010)
#define SUPPORTED_FEATURES ( \
      FEATURE_MEMBER_OF_ALL_GROUPS \
    | FEATURE_MANUAL_SOURCE_ROUTE \
)

#define MAX_EPHEMERAL_SOURCE_ROUTES  (20)
uint16_t manual_source_routes[MAX_EPHEMERAL_SOURCE_ROUTES][1 + EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT + 1];


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
  for (uint8_t i = 0; i < MAX_EPHEMERAL_SOURCE_ROUTES; i++) {
    for (uint8_t j = 0; j < EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT; j++) {
      manual_source_routes[i][j] = EMBER_NULL_NODE_ID;
    }
  }
}

/** @brief Packet filter callback
 *
 * Filters and/or mutates incoming packets. Currently used only for wildcard multicast
 * group membership.
 */
EmberPacketAction emberAfIncomingPacketFilterCallback(EmberZigbeePacketType packetType,
                                                      uint8_t* packetData,
                                                      uint8_t* size_p,
                                                      void* data)
{
  if ((packetType == EMBER_ZIGBEE_PACKET_TYPE_APS_DATA) && (*size_p >= 3)) {
    uint8_t deliveryMode = (packetData[0] & 0b00001100) >> 2;

    // Ensure we automatically "join" every multicast group
    if (deliveryMode == 0x03) {
      // Take ownership over the first entry and continuously rewrite it
      EmberMulticastTableEntry *tableEntry = &(sl_zigbee_get_multicast_table()[0]);

      tableEntry->endpoint = 1;
      tableEntry->multicastId = (packetData[2] >> 8) | (packetData[1] >> 0);
      tableEntry->networkIndex = 0;
    }
  }

  return EMBER_ACCEPT_PACKET;
}


void nc_zigbee_override_append_source_route(EmberNodeId destination,
                                            EmberMessageBuffer *header,
                                            bool *consumed)
{
  uint8_t index = 0xFF;

  for (uint8_t i = 0; i < MAX_EPHEMERAL_SOURCE_ROUTES; i++) {
    if (manual_source_routes[i][0] == destination) {
      index = i;
      break;
    }
  }

  if (index == 0xFF) {
    *consumed = false;
    return;
  }

  uint16_t *routes = &manual_source_routes[index][1];

  uint8_t relay_count = 0;
  uint8_t relay_index = 0;

  while (routes[relay_count] != EMBER_NULL_NODE_ID) {
    relay_count++;
  }

  *consumed = true;
  emberAppendToLinkedBuffers(*header, &relay_count, 1);
  emberAppendToLinkedBuffers(*header, &relay_index, 1);

  //emberExtendLinkedBuffer(*header, relay_count * 2);
  //memcpy(2 + emberMessageBufferContents(*header), routes, relay_count * 2);

  for (uint8_t i = 0; i < relay_count; i++) {
    emberAppendToLinkedBuffers(*header, &routes[i], 2);
  }

  return;
}


EmberStatus emberAfPluginXncpIncomingCustomFrameCallback(uint8_t messageLength,
                                                         uint8_t *messagePayload,
                                                         uint8_t *replyPayloadLength,
                                                         uint8_t *replyPayload) {
  *replyPayloadLength = 0;

  if (messageLength < 2) {
    return EMBER_BAD_ARGUMENT;
  }

  uint16_t command_id = (messagePayload[0] << 0) | (messagePayload[1] << 8);

  switch (command_id) {
    case XNCP_CMD_GET_SUPPORTED_FEATURES:
      *replyPayloadLength += 4;
      replyPayload[0] = (uint8_t)((SUPPORTED_FEATURES >>  0) & 0xFF);
      replyPayload[1] = (uint8_t)((SUPPORTED_FEATURES >>  8) & 0xFF);
      replyPayload[2] = (uint8_t)((SUPPORTED_FEATURES >> 16) & 0xFF);
      replyPayload[3] = (uint8_t)((SUPPORTED_FEATURES >> 24) & 0xFF);
      break;

    case XNCP_CMD_SET_SOURCE_ROUTE:
      if ((messageLength < 4) || (messageLength % 2 != 0)) {
        return EMBER_BAD_ARGUMENT;
      }

      uint8_t num_relays = (messageLength - 4) / 2;

      if (num_relays > EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT + 1) {
        return EMBER_BAD_ARGUMENT;
      }

      // If we don't find a better index, pick one at random
      uint8_t insertion_index = emberGetPseudoRandomNumber() % MAX_EPHEMERAL_SOURCE_ROUTES;
      uint16_t node_id = (messagePayload[2] << 0) | (messagePayload[3] << 8);

      for (uint8_t i = 0; i < MAX_EPHEMERAL_SOURCE_ROUTES; i++) {
        uint16_t current_node_id = manual_source_routes[i][0];

        if (current_node_id == EMBER_NULL_NODE_ID) {
          insertion_index = i;
        } else if (current_node_id == node_id) {
          insertion_index = i;
          break;
        }
      }

      manual_source_routes[insertion_index][0] = node_id;
      manual_source_routes[insertion_index][1 + num_relays] = EMBER_NULL_NODE_ID;

      for (uint8_t i = 0; i < num_relays; i++) {
        manual_source_routes[insertion_index][1 + i] = (
            (messagePayload[4 + i * 2 + 0] << 0)
          | (messagePayload[4 + i * 2 + 1] << 8)
        );
      }
      break;

    default:
      return EMBER_BAD_ARGUMENT;
  }

  return EMBER_SUCCESS;
}
