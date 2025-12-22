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

#endif // ZBT2_ROUTER_CALLBACKS_H
