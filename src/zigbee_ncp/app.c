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

#include "stack/include/message.h"

#include "config/xncp_config.h"

#define BUILD_UINT16(low, high)  (((uint16_t)(low) << 0) | ((uint16_t)(high) << 8))

typedef enum {
  XNCP_CMD_GET_SUPPORTED_FEATURES = 0x0000,
  XNCP_CMD_SET_SOURCE_ROUTE = 0x0001,
  XNCP_CMD_GET_BOARD_NAME_OVERRIDE = 0x0002,
  XNCP_CMD_GET_MANUF_NAME_OVERRIDE = 0x0003
} CUSTOM_EZSP_CMD;


#define FEATURE_MEMBER_OF_ALL_GROUPS  (0b00000000000000000000000000000001)
#define FEATURE_MANUAL_SOURCE_ROUTE   (0b00000000000000000000000000000010)
#define FEATURE_BOARD_MANUF_OVERRIDE  (0b00000000000000000000000000000100)
#define SUPPORTED_FEATURES ( \
      FEATURE_MEMBER_OF_ALL_GROUPS \
    | FEATURE_MANUAL_SOURCE_ROUTE \
    | FEATURE_BOARD_MANUF_OVERRIDE \
)


typedef struct ManualSourceRoute {
  bool active;
  uint16_t destination;
  uint8_t num_relays;
  uint16_t relays[EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT];
} ManualSourceRoute;

ManualSourceRoute manual_source_routes[XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE];

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
      tableEntry->multicastId = BUILD_UINT16(packetData[1], packetData[2]);
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

  for (uint8_t i = 0; i < XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE; i++) {
    if (manual_source_routes[i].active && (manual_source_routes[i].destination == destination)) {
      index = i;
      break;
    }
  }

  if (index == 0xFF) {
    *consumed = false;
    return;
  }

  ManualSourceRoute *route = &manual_source_routes[index];

  uint8_t relay_index = 0;

  *consumed = true;
  route->active = false;  // Disable the route after a single use

  emberAppendToLinkedBuffers(*header, &route->num_relays, 1);
  emberAppendToLinkedBuffers(*header, &relay_index, 1);

  for (uint8_t i = 0; i < route->num_relays; i++) {
    emberAppendToLinkedBuffers(*header, (uint8_t*)&route->relays[i], 2);
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

  uint16_t command_id = BUILD_UINT16(messagePayload[0], messagePayload[1]);

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

      // If we don't find a better index, pick one at random to replace
      uint8_t insertion_index = emberGetPseudoRandomNumber() % XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE;
      uint16_t node_id = BUILD_UINT16(messagePayload[2], messagePayload[3]);

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
        uint16_t relay = BUILD_UINT16(messagePayload[4 + 2 * i + 0], messagePayload[4 + 2 * i + 1]);
        route->relays[i] = relay;
      }

      route->destination = node_id;
      route->num_relays = num_relays;
      route->active = true;

      break;

    case XNCP_CMD_GET_BOARD_NAME_OVERRIDE:
      if (!XNCP_BOARD_MANUF_OVERRIDE_ENABLED) {
        break;
      }

      uint8_t length = strlen(XNCP_BOARD_NAME_OVERRIDE);
      *replyPayloadLength += length;
      memcpy(replyPayload, XNCP_BOARD_NAME_OVERRIDE, length);
      break;

    case XNCP_CMD_GET_MANUF_NAME_OVERRIDE:
      if (!XNCP_BOARD_MANUF_OVERRIDE_ENABLED) {
        break;
      }

      uint8_t length = strlen(XNCP_MANUF_NAME_OVERRIDE);
      *replyPayloadLength += length;
      memcpy(replyPayload, XNCP_MANUF_NAME_OVERRIDE, length);
      break;

    default:
      return EMBER_BAD_ARGUMENT;
  }

  return EMBER_SUCCESS;
}
