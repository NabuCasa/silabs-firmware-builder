#ifndef ROUTER_NVRAM_RESET_H
#define ROUTER_NVRAM_RESET_H

#include <stdint.h>

/**
 * @brief Initialize router NVRAM reset functionality
 *
 * This function is called during system initialization to check for non-router
 * NVRAM state and factory reset if found. This handles the case where
 * coordinator/NCP firmware was previously flashed and we need to wipe the
 * network state before starting as a router.
 *
 * @param init_level Initialization level (unused)
 */
void router_nvram_reset_init(uint8_t init_level);

#endif // ROUTER_NVRAM_RESET_H
