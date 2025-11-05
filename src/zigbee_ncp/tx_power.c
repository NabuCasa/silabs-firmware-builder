#include <stdint.h>
#include <stdbool.h>

typedef struct {
  char code[2];
  int8_t recommended_power_dbm;
  int8_t max_power_dbm;
} CountryTxPower;

static const CountryTxPower COUNTRY_TX_POWERS[] = {
  // EU Member States
  {{'A', 'T'}, 10, 10},  // Austria
  {{'B', 'E'}, 10, 10},  // Belgium
  {{'B', 'G'}, 10, 10},  // Bulgaria
  {{'H', 'R'}, 10, 10},  // Croatia
  {{'C', 'Y'}, 10, 10},  // Cyprus
  {{'C', 'Z'}, 10, 10},  // Czech Republic
  {{'D', 'K'}, 10, 10},  // Denmark
  {{'E', 'E'}, 10, 10},  // Estonia
  {{'F', 'I'}, 10, 10},  // Finland
  {{'F', 'R'}, 10, 10},  // France
  {{'D', 'E'}, 10, 10},  // Germany
  {{'G', 'R'}, 10, 10},  // Greece
  {{'H', 'U'}, 10, 10},  // Hungary
  {{'I', 'E'}, 10, 10},  // Ireland
  {{'I', 'T'}, 10, 10},  // Italy
  {{'L', 'V'}, 10, 10},  // Latvia
  {{'L', 'T'}, 10, 10},  // Lithuania
  {{'L', 'U'}, 10, 10},  // Luxembourg
  {{'M', 'T'}, 10, 10},  // Malta
  {{'N', 'L'}, 10, 10},  // Netherlands
  {{'P', 'L'}, 10, 10},  // Poland
  {{'P', 'T'}, 10, 10},  // Portugal
  {{'R', 'O'}, 10, 10},  // Romania
  {{'S', 'K'}, 10, 10},  // Slovakia
  {{'S', 'I'}, 10, 10},  // Slovenia
  {{'E', 'S'}, 10, 10},  // Spain
  {{'S', 'E'}, 10, 10},  // Sweden
  // EEA Members
  {{'I', 'S'}, 10, 10},  // Iceland
  {{'L', 'I'}, 10, 10},  // Liechtenstein
  {{'N', 'O'}, 10, 10},  // Norway
  // Standards harmonized with RED or ETSI
  {{'C', 'H'}, 10, 10},  // Switzerland
  {{'G', 'B'}, 10, 10},  // United Kingdom
  {{'T', 'R'}, 10, 10},  // Turkey
  {{'A', 'L'}, 10, 10},  // Albania
  {{'B', 'A'}, 10, 10},  // Bosnia and Herzegovina
  {{'G', 'E'}, 10, 10},  // Georgia
  {{'M', 'D'}, 10, 10},  // Moldova
  {{'M', 'E'}, 10, 10},  // Montenegro
  {{'M', 'K'}, 10, 10},  // North Macedonia
  {{'R', 'S'}, 10, 10},  // Serbia
  {{'U', 'A'}, 10, 10},  // Ukraine
  // Other CEPT nations
  {{'A', 'D'}, 10, 10},  // Andorra
  {{'A', 'Z'}, 10, 10},  // Azerbaijan
  {{'M', 'C'}, 10, 10},  // Monaco
  {{'S', 'M'}, 10, 10},  // San Marino
  {{'V', 'A'}, 10, 10},  // Vatican City
  // Disable the maximum, for testing
  {{'?', '?'}, 8, 127},
};

void get_tx_power_for_country(const char c1, const char c2, CountryTxPower *output) {
  output.code[0] = c1;
  output.code[1] = c2;

  for (uint8_t i = 0; i < sizeof(COUNTRY_TX_POWERS) / sizeof(CountryTxPower); i++) {
    if (COUNTRY_TX_POWERS[i].code[0] == c1 && COUNTRY_TX_POWERS[i].code[1] == c2) {
      output.recommended_power_dbm = COUNTRY_TX_POWERS[i].recommended_power_dbm;
      output.max_power_dbm = COUNTRY_TX_POWERS[i].max_power_dbm;

      return;
    }
  }

  // Not found, return default
  output.recommended_power_dbm = XNCP_DEFAULT_MAX_TX_POWER_DBM;
  output.max_power_dbm = XNCP_DEFAULT_MAX_TX_POWER_DBM;
}
