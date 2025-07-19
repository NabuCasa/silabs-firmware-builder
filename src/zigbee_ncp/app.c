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
#include "app/ncp/plugin/xncp/xncp.h"
#include "em_usart.h"

#include "config/xncp_config.h"
#include "config/sl_iostream_usart_vcom_config.h"


#define BUILD_UINT16(low, high)  (((uint16_t)(low) << 0) | ((uint16_t)(high) << 8))

#define FEATURE_MEMBER_OF_ALL_GROUPS    (0b00000000000000000000000000000001)
#define FEATURE_MANUAL_SOURCE_ROUTE     (0b00000000000000000000000000000010)
#define FEATURE_MFG_TOKEN_OVERRIDES     (0b00000000000000000000000000000100)
#define FEATURE_BUILD_STRING            (0b00000000000000000000000000001000)
#define FEATURE_FLOW_CONTROL_TYPE       (0b00000000000000000000000000010000)
#define FEATURE_RESTORE_ROUTE_TABLE     (0b00000000000000000000000000100000)

#define SUPPORTED_FEATURES ( \
      FEATURE_MEMBER_OF_ALL_GROUPS \
    | FEATURE_MANUAL_SOURCE_ROUTE \
    | FEATURE_MFG_TOKEN_OVERRIDES \
    | FEATURE_BUILD_STRING \
    | FEATURE_FLOW_CONTROL_TYPE \
    | FEATURE_RESTORE_ROUTE_TABLE \
)

extern sli_zigbee_route_table_entry_t sli_zigbee_route_table[];
extern uint8_t sli_zigbee_route_table_size;

/*
 * Indicates whether this entry is active (0), being
 * discovered (1), unused (3), or validating (4).
 */
typedef enum {
  EMBER_ROUTE_ACTIVE = 0,
  EMBER_ROUTE_BEING_DISCOVERED = 1,
  EMBER_ROUTE_UNUSED = 3,
  EMBER_ROUTE_VALIDATING = 4
} EmberRouteStatus;


typedef enum {
  XNCP_FLOW_CONTROL_TYPE_SOFTWARE = 0x00,
  XNCP_FLOW_CONTROL_TYPE_HARDWARE = 0x01,
} XncpFlowControlType;

typedef enum {
  XNCP_CMD_GET_SUPPORTED_FEATURES_REQ = 0x0000,
  XNCP_CMD_SET_SOURCE_ROUTE_REQ       = 0x0001,
  XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ = 0x0002,
  XNCP_CMD_GET_BUILD_STRING_REQ       = 0x0003,
  XNCP_CMD_GET_FLOW_CONTROL_TYPE_REQ  = 0x0004,
  XNCP_CMD_SET_ROUTE_TABLE_ENTRY_REQ  = 0x0005,
  XNCP_CMD_GET_ROUTE_TABLE_ENTRY_REQ  = 0x0006,

  XNCP_CMD_GET_SUPPORTED_FEATURES_RSP = XNCP_CMD_GET_SUPPORTED_FEATURES_REQ | 0x8000,
  XNCP_CMD_SET_SOURCE_ROUTE_RSP       = XNCP_CMD_SET_SOURCE_ROUTE_REQ       | 0x8000,
  XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_RSP = XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ | 0x8000,
  XNCP_CMD_GET_BUILD_STRING_RSP       = XNCP_CMD_GET_BUILD_STRING_REQ       | 0x8000,
  XNCP_CMD_GET_FLOW_CONTROL_TYPE_RSP  = XNCP_CMD_GET_FLOW_CONTROL_TYPE_REQ  | 0x8000,
  XNCP_CMD_SET_ROUTE_TABLE_ENTRY_RSP  = XNCP_CMD_SET_ROUTE_TABLE_ENTRY_REQ  | 0x8000,
  XNCP_CMD_GET_ROUTE_TABLE_ENTRY_RSP  = XNCP_CMD_GET_ROUTE_TABLE_ENTRY_REQ  | 0x8000,

  XNCP_CMD_UNKNOWN = 0xFFFF
} XncpCommand;

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


sli_zigbee_route_table_entry_t* find_free_routing_table_entry(EmberNodeId destination) {
  uint8_t index = 0xFF;

  sli_zigbee_route_table_entry_t *route_table_entry = NULL;

  for (uint8_t i = 0; i < sli_zigbee_route_table_size; i++) {
    route_table_entry = &sli_zigbee_route_table[i];

    if (route_table_entry->destination == destination) {
      // Perfer a route to the destination, if one exists
      return route_table_entry;
    } else if (route_table_entry->status == EMBER_ROUTE_UNUSED) {
      // Otherwise, keep track of the most recent unused route
      index = i;
      continue;
    }
  }

  // If we have no free route slots, we still need to insert somewhere. Pick at random.
  if (index == 0xFF) {
    index = emberGetPseudoRandomNumber() % sli_zigbee_route_table_size;
  }

  return &sli_zigbee_route_table[index];
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

bool __wrap_sli_zigbee_am_multicast_member(EmberMulticastId multicastId)
{
  // Ignore all binding and multicast table logic, we want all group packets
  return true;
}


void nc_zigbee_override_append_source_route(EmberNodeId destination,
                                            EmberMessageBuffer *header,
                                            bool *consumed)
{
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

  emberAppendToLinkedBuffers(*header, &route->num_relays, 1);
  emberAppendToLinkedBuffers(*header, &relay_index, 1);

  for (uint8_t i = 0; i < route->num_relays; i++) {
    emberAppendToLinkedBuffers(*header, (uint8_t*)&route->relays[route->num_relays - i - 1], 2);
  }

  return;
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

    case XNCP_CMD_GET_BUILD_STRING_REQ: {
      rsp_command_id = XNCP_CMD_GET_BUILD_STRING_RSP;
      rsp_status = EMBER_SUCCESS;

      uint8_t value_length = strlen(XNCP_BUILD_STRING);
      memcpy(replyPayload + *replyPayloadLength, XNCP_BUILD_STRING, value_length);
      *replyPayloadLength += value_length;
      break;
    }

    case XNCP_CMD_GET_FLOW_CONTROL_TYPE_REQ: {
      rsp_command_id = XNCP_CMD_GET_FLOW_CONTROL_TYPE_RSP;
      rsp_status = EMBER_SUCCESS;

      XncpFlowControlType flow_control_type;

      switch (SL_IOSTREAM_USART_VCOM_FLOW_CONTROL_TYPE) {
        case usartHwFlowControlCtsAndRts: {
          flow_control_type = XNCP_FLOW_CONTROL_TYPE_HARDWARE;
          break;
        }

        case usartHwFlowControlNone: {
          flow_control_type = XNCP_FLOW_CONTROL_TYPE_SOFTWARE;
          break;
        }

        default: {
          flow_control_type = XNCP_FLOW_CONTROL_TYPE_SOFTWARE;
          break;
        }
      }

      replyPayload[(*replyPayloadLength)++] = (uint8_t)(flow_control_type & 0xFF);
      break;
    }

    case XNCP_CMD_SET_ROUTE_TABLE_ENTRY_REQ: {
      rsp_command_id = XNCP_CMD_SET_ROUTE_TABLE_ENTRY_RSP;
      rsp_status = EMBER_SUCCESS;

      if (messageLength != 7) {
        rsp_status = EMBER_BAD_ARGUMENT;
        break;
      }

      uint8_t route_table_index = messagePayload[0];
      if (route_table_index >= sli_zigbee_route_table_size) {
        rsp_status = EMBER_BAD_ARGUMENT;
        break;
      }

      sli_zigbee_route_table_entry_t *entry = &sli_zigbee_route_table[route_table_index];
      entry->destination = BUILD_UINT16(messagePayload[1], messagePayload[2]);
      entry->nextHop = BUILD_UINT16(messagePayload[3], messagePayload[4]);
      entry->status = messagePayload[5];
      entry->cost = messagePayload[6];
      entry->networkIndex = 0;
      break;
    }

    case XNCP_CMD_GET_ROUTE_TABLE_ENTRY_REQ: {
      rsp_command_id = XNCP_CMD_GET_ROUTE_TABLE_ENTRY_RSP;
      rsp_status = EMBER_SUCCESS;

      if (messageLength != 1) {
        rsp_status = EMBER_BAD_ARGUMENT;
        break;
      }

      uint8_t route_table_index = messagePayload[0];
      if (route_table_index >= sli_zigbee_route_table_size) {
        rsp_status = EMBER_BAD_ARGUMENT;
        break;
      }

      sli_zigbee_route_table_entry_t *entry = &sli_zigbee_route_table[route_table_index];

      replyPayload[(*replyPayloadLength)++] = (entry->destination & 0x00FF) >> 0;
      replyPayload[(*replyPayloadLength)++] = (entry->destination & 0xFF00) >> 8;

      replyPayload[(*replyPayloadLength)++] = (entry->nextHop & 0x00FF) >> 0;
      replyPayload[(*replyPayloadLength)++] = (entry->nextHop & 0xFF00) >> 8;

      replyPayload[(*replyPayloadLength)++] = entry->status;
      replyPayload[(*replyPayloadLength)++] = entry->cost;
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
