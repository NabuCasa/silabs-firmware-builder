/*
 * zbt2_router_callbacks.h
 *
 * ZBT-2 Router Callbacks
 * Handles Zigbee router callbacks for ZBT-2 hardware
 */

#ifndef ZBT2_ROUTER_CALLBACKS_H
#define ZBT2_ROUTER_CALLBACKS_H

#include <stdint.h>
#include "sl_status.h"

/**
 * @brief Initialize the ZBT-2 router callbacks
 * Called during system initialization
 */
void zbt2_router_init(uint8_t init_level);

/**
 * @brief Handle stack status changes
 * @param status The new stack status
 */
void zbt2_router_stack_status_cb(sl_status_t status);

/**
 * @brief Handle network steering completion
 * @param status Result status
 * @param totalBeacons Total beacons heard
 * @param joinAttempts Number of join attempts
 * @param finalState Final steering state
 */
void zbt2_router_network_steering_complete_cb(sl_status_t status,
                                               uint8_t totalBeacons,
                                               uint8_t joinAttempts,
                                               uint8_t finalState);

#endif // ZBT2_ROUTER_CALLBACKS_H
