#ifndef TX_POWER_H
#define TX_POWER_H

#include <stdint.h>
#include "config/xncp_config.h"

typedef struct {
  char code[2];
  int8_t recommended_power_dbm;
  int8_t max_power_dbm;
} CountryTxPower;

void get_tx_power_for_country(const char c1, const char c2, CountryTxPower *output);

#endif // TX_POWER_H
