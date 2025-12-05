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
#include "stack/include/message.h"

// Forward declaration for source route override from XNCP common extension
extern void xncp_source_route_override_append(uint16_t destination,
                                               void *header,
                                               bool *consumed);

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

/** @brief
 *
 * Override to accept all multicast group packets
 */
bool __wrap_sli_zigbee_am_multicast_member(EmberMulticastId multicastId)
{
  (void)multicastId;
  // Ignore all binding and multicast table logic, we want all group packets
  return true;
}

/** @brief
 *
 * Override source route handling - forwards to XNCP common extension
 */
void nc_zigbee_override_append_source_route(EmberNodeId destination,
                                            EmberMessageBuffer *header,
                                            bool *consumed)
{
  xncp_source_route_override_append(destination, header, consumed);
}
