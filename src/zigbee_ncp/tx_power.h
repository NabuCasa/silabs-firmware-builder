#ifndef TX_POWER_H
#define TX_POWER_H

#include <stdint.h>
#include "config/xncp_config.h"

/**
 * Get the maximum TX power for a given country code.
 *
 * @param country_code Two-character uppercase country code (e.g., "US", "DE")
 * @return TX power in dBm, or `XNCP_DEFAULT_MAX_TX_POWER_DBM` if country not found
 */
int8_t get_max_tx_power_for_country(const char c1, const char c2);

#endif // TX_POWER_H
