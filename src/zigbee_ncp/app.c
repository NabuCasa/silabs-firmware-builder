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
  XNCP_CMD_GET_SUPPORTED_FEATURES = 0x00,
} CUSTOM_EZSP_CMD;


#define FEATURE_MEMBER_OF_ALL_GROUPS (0b00000000000000000000000000000001)
#define SUPPORTED_FEATURES (FEATURE_MEMBER_OF_ALL_GROUPS)


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
  if ((packetType == EMBER_ZIGBEE_PACKET_TYPE_APS_COMMAND) && (*size_p >= 3)) {
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


EmberStatus emberAfPluginXncpIncomingCustomFrameCallback(uint8_t messageLength,
                                                         uint8_t *messagePayload,
                                                         uint8_t *replyPayloadLength,
                                                         uint8_t *replyPayload) {
  *replyPayloadLength = 0;

  if (messageLength < 1) {
    return EMBER_BAD_ARGUMENT;
  }

  switch (messagePayload[0]) {
    case XNCP_CMD_GET_SUPPORTED_FEATURES:
      *replyPayloadLength += 4;
      replyPayload[0] = (uint8_t)((SUPPORTED_FEATURES >>  0) & 0xFF);
      replyPayload[1] = (uint8_t)((SUPPORTED_FEATURES >>  8) & 0xFF);
      replyPayload[2] = (uint8_t)((SUPPORTED_FEATURES >> 16) & 0xFF);
      replyPayload[3] = (uint8_t)((SUPPORTED_FEATURES >> 24) & 0xFF);
      break;
    default:
      return EMBER_BAD_ARGUMENT;
  }

  return EMBER_SUCCESS;
}
