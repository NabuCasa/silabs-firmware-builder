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

#define FEATURE_MEMBER_OF_ALL_GROUPS  (0b00000000000000000000000000000001)
#define FEATURE_MANUAL_SOURCE_ROUTE   (0b00000000000000000000000000000010)
#define FEATURE_MFG_TOKEN_OVERRIDES   (0b00000000000000000000000000000100)
#define SUPPORTED_FEATURES ( \
      FEATURE_MEMBER_OF_ALL_GROUPS \
    | FEATURE_MANUAL_SOURCE_ROUTE \
    | FEATURE_MFG_TOKEN_OVERRIDES \
)


typedef enum {
  XNCP_CMD_GET_SUPPORTED_FEATURES_REQ = 0x0000,
  XNCP_CMD_SET_SOURCE_ROUTE_REQ       = 0x0001,
  XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ = 0x0002,

  XNCP_CMD_GET_SUPPORTED_FEATURES_RSP = XNCP_CMD_GET_SUPPORTED_FEATURES_REQ | 0x8000,
  XNCP_CMD_SET_SOURCE_ROUTE_RSP       = XNCP_CMD_SET_SOURCE_ROUTE_REQ       | 0x8000,
  XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_RSP = XNCP_CMD_GET_MFG_TOKEN_OVERRIDE_REQ | 0x8000,

  XNCP_CMD_UNKNOWN = 0xFFFF
} XNCP_COMMAND;

typedef struct ManualSourceRoute {
  bool active;
  uint16_t destination;
  uint8_t num_relays;
  uint16_t relays[EMBER_MAX_SOURCE_ROUTE_RELAY_COUNT];
} ManualSourceRoute;

ManualSourceRoute manual_source_routes[XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE];


typedef struct EnqueuedEvent {
  bool sent;
  uint8_t length;
  uint8_t message[64];
} EnqueuedEvent;


#define NC_MAX_ENQUEUED_EVENTS (8)
EnqueuedEvent nc_enqueued_events[NC_MAX_ENQUEUED_EVENTS];


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

  for (uint8_t i = 0; i < NC_MAX_ENQUEUED_EVENTS; i++) {
    nc_enqueued_events[i].sent = true;
  }
}

void emberAfMainTickCallback(void)
{
  for (uint8_t i = 0; i < NC_MAX_ENQUEUED_EVENTS; i++) {
    EnqueuedEvent *event = &nc_enqueued_events[i];

    if (event->sent) {
      continue;
    }

    emberAfPluginXncpSendCustomEzspMessage(event->length, event->message);
    event->sent = true;
  }
}

/** @brief Incoming packet filter callback
 *
 * Filters and/or mutates incoming packets. Currently used only for wildcard multicast
 * group membership.
 */
EmberPacketAction sli_zigbee_af_packet_handoff_incoming_callback(EmberZigbeePacketType packetType,
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


/** @brief Outgoing packet filter callback
 *
 * Filters and/or mutates outgoing packets.
 */
EmberPacketAction sli_zigbee_af_packet_handoff_outgoing_callback(EmberZigbeePacketType packetType,
                                                                 EmberMessageBuffer packetBuffer,
                                                                 uint8_t index,
                                                                 void *data)
{
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

void nc_enqueue_event(uint8_t length, char* message) {
  for (uint8_t i = 0; i < NC_MAX_ENQUEUED_EVENTS; i++) {
    EnqueuedEvent *event = &nc_enqueued_events[i];

    if (!event->sent) {
      continue;
    }

    event->sent = false;
    event->length = length;
    memcpy(event->message, message, length);
    break;
  }
}


void nc_incoming_message(void* first_arg, ...)                   { nc_enqueue_event(16, "incoming_message"); }
void nc_message_sent(void* first_arg, ...)                       { nc_enqueue_event(12, "message_sent"); }
void nc_trust_center_join(void* first_arg, ...)                  { nc_enqueue_event(17, "trust_center_join"); }
void nc_mark_buffers(void* first_arg, ...)                       {}//{ nc_enqueue_event(12, "mark_buffers"); }
void nc_packet_handoff_incoming(void* first_arg, ...)            { nc_enqueue_event(23, "packet_handoff_incoming"); }
void nc_packet_handoff_outgoing(void* first_arg, ...)            { nc_enqueue_event(23, "packet_handoff_outgoing"); }
void nc_incoming_mfg_test_message(void* first_arg, ...)          { nc_enqueue_event(25, "incoming_mfg_test_message"); }
void nc_stack_status(void* first_arg, ...)                       { nc_enqueue_event(12, "stack_status"); }
void nc_redirect_outgoing_message(void* first_arg, ...)          { nc_enqueue_event(25, "redirect_outgoing_message"); }
void nc_energy_scan_result(void* first_arg, ...)                 { nc_enqueue_event(18, "energy_scan_result"); }
void nc_network_found(void* first_arg, ...)                      { nc_enqueue_event(13, "network_found"); }
void nc_scan_complete(void* first_arg, ...)                      { nc_enqueue_event(13, "scan_complete"); }
void nc_unused_pan_id_found(void* first_arg, ...)                { nc_enqueue_event(19, "unused_pan_id_found"); }
void nc_child_join(void* first_arg, ...)                         { nc_enqueue_event(10, "child_join"); }
void nc_duty_cycle(void* first_arg, ...)                         { nc_enqueue_event(10, "duty_cycle"); }
void nc_remote_set_binding(void* first_arg, ...)                 { nc_enqueue_event(18, "remote_set_binding"); }
void nc_remote_delete_binding(void* first_arg, ...)              { nc_enqueue_event(21, "remote_delete_binding"); }
void nc_poll_complete(void* first_arg, ...)                      { nc_enqueue_event(13, "poll_complete"); }
void nc_poll(void* first_arg, ...)                               { nc_enqueue_event(4, "poll"); }
void nc_debug(void* first_arg, ...)                              { nc_enqueue_event(5, "debug"); }
void nc_incoming_many_to_one_route_request(void* first_arg, ...) { nc_enqueue_event(34, "incoming_many_to_one_route_request"); }
void nc_incoming_route_error(void* first_arg, ...)               { nc_enqueue_event(20, "incoming_route_error"); }
void nc_incoming_network_status(void* first_arg, ...)            { nc_enqueue_event(23, "incoming_network_status"); }
void nc_incoming_route_record(void* first_arg, ...)              { nc_enqueue_event(21, "incoming_route_record"); }
void nc_id_conflict(void* first_arg, ...)                        { nc_enqueue_event(11, "id_conflict"); }
void nc_mac_passthrough_message(void* first_arg, ...)            { nc_enqueue_event(23, "mac_passthrough_message"); }
void nc_stack_token_changed(void* first_arg, ...)                { nc_enqueue_event(19, "stack_token_changed"); }
void nc_timer(void* first_arg, ...)                              { nc_enqueue_event(5, "timer"); }
void nc_counter_rollover(void* first_arg, ...)                   { nc_enqueue_event(16, "counter_rollover"); }
void nc_raw_transmit_complete(void* first_arg, ...)              { nc_enqueue_event(21, "raw_transmit_complete"); }
void nc_switch_network_key(void* first_arg, ...)                 { nc_enqueue_event(18, "switch_network_key"); }
void nc_zigbee_key_establishment(void* first_arg, ...)           { nc_enqueue_event(24, "zigbee_key_establishment"); }
void nc_generate_cbke_keys(void* first_arg, ...)                 { nc_enqueue_event(18, "generate_cbke_keys"); }
void nc_calculate_smacs(void* first_arg, ...)                    { nc_enqueue_event(15, "calculate_smacs"); }
void nc_dsa_sign(void* first_arg, ...)                           { nc_enqueue_event(8, "dsa_sign"); }
void nc_dsa_verify(void* first_arg, ...)                         { nc_enqueue_event(10, "dsa_verify"); }
void nc_incoming_bootload_message(void* first_arg, ...)          { nc_enqueue_event(25, "incoming_bootload_message"); }
void nc_bootload_transmit_complete(void* first_arg, ...)         { nc_enqueue_event(26, "bootload_transmit_complete"); }
void nc_zll_network_found(void* first_arg, ...)                  { nc_enqueue_event(17, "zll_network_found"); }
void nc_zll_scan_complete(void* first_arg, ...)                  { nc_enqueue_event(17, "zll_scan_complete"); }
void nc_zll_address_assignment(void* first_arg, ...)             { nc_enqueue_event(22, "zll_address_assignment"); }
void nc_zll_touch_link_target(void* first_arg, ...)              { nc_enqueue_event(21, "zll_touch_link_target"); }
void nc_mac_filter_match_message(void* first_arg, ...)           { nc_enqueue_event(24, "mac_filter_match_message"); }
void nc_d_gp_sent(void* first_arg, ...)                          { nc_enqueue_event(9, "d_gp_sent"); }
void nc_pan_id_conflict(void* first_arg, ...)                    { nc_enqueue_event(15, "pan_id_conflict"); }
void nc_orphan_notification(void* first_arg, ...)                { nc_enqueue_event(19, "orphan_notification"); }
void nc_counter(void* first_arg, ...)                            {}//{ nc_enqueue_event(7, "counter"); }
void nc_mac_passthrough_filter(void* first_arg, ...)             { nc_enqueue_event(22, "mac_passthrough_filter"); }
void nc_generate_cbke_keys_handler283k1(void* first_arg, ...)    { nc_enqueue_event(31, "generate_cbke_keys_handler283k1"); }
void nc_calculate_smacs_handler283k1(void* first_arg, ...)       { nc_enqueue_event(28, "calculate_smacs_handler283k1"); }
void nc_gpep_incoming_message(void* first_arg, ...)              { nc_enqueue_event(21, "gpep_incoming_message"); }
void nc_rtos_idle(void* first_arg, ...)                          { nc_enqueue_event(9, "rtos_idle"); }
void nc_rtos_stack_wakeup_isr(void* first_arg, ...)              { nc_enqueue_event(21, "rtos_stack_wakeup_isr"); }
void nc_radio_needs_calibrating(void* first_arg, ...)            { nc_enqueue_event(23, "radio_needs_calibrating"); }
void nc_scan_error(void* first_arg, ...)                         { nc_enqueue_event(10, "scan_error"); }

const char * COUNTER_STRINGS[] = {EMBER_COUNTER_STRINGS};

void emberAfCounterCallback(
       // Type of Counter
       EmberCounterType type,
       // Counter Info and value
       EmberCounterInfo info)
{
  uint8_t buffer[64] = {0};
  strcpy((char*)buffer, COUNTER_STRINGS[type]);
  uint8_t len = strlen(buffer);
  buffer[len++] = " ";

  EmberNodeId destinationNodeId;

  if (emberCounterRequiresDestinationNodeId(type) && emberCounterRequiresPhyIndex(type)) {
    destinationNodeId = ((EmberExtraCounterInfo *) (info.otherFields))->destinationNodeId;
  } else if (emberCounterRequiresDestinationNodeId(type)) {
    destinationNodeId = *((EmberNodeId*)info.otherFields);
  } else {
    destinationNodeId = 0xFFFF;
  }

  memcpy(buffer + len, &destinationNodeId, sizeof(destinationNodeId));

  nc_enqueue_event(sizeof(buffer), &buffer);
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
