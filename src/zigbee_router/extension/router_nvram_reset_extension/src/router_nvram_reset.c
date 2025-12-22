#include "router_nvram_reset.h"
#include "nvm3_default.h"
#include "psa/crypto.h"
#include "sl_token_api.h"

#ifdef STACK_TYPES_HEADER
#include "stack/include/sl_zigbee_types.h"
#else
#include "stack/include/ember-types.h"
#endif

#include <stdint.h>

#define ZB_PSA_KEY_ID_MIN  0x00030000
#define ZB_PSA_KEY_ID_MAX  0x0003FFFF

// Check for non-router NVRAM state and factory reset if found.
// This handles the case where coordinator/NCP firmware was previously flashed
// and we need to wipe the network state before starting as a router.
static void check_and_reset_non_router_state(void)
{
  tokTypeStackNodeData nodeData;
  halCommonGetToken(&nodeData, TOKEN_STACK_NODE_DATA);

  if (nodeData.panId != 0xFFFF &&
      nodeData.nodeType != SL_ZIGBEE_ROUTER &&
      nodeData.nodeType != SL_ZIGBEE_UNKNOWN_DEVICE) {

    nvm3_initDefault();
    nvm3_eraseAll(nvm3_defaultHandle);

    for (psa_key_id_t key_id = ZB_PSA_KEY_ID_MIN; key_id <= ZB_PSA_KEY_ID_MAX; key_id++) {
      psa_destroy_key(key_id);
    }

    NVIC_SystemReset();
  }
}

void router_nvram_reset_init(uint8_t init_level)
{
  (void)init_level;
  check_and_reset_non_router_state();
}
