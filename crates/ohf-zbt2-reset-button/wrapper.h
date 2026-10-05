/* Button + sleeptimer + reset-backend surface for the pin-hole reset button. The allowlist
 * in build.rs controls what is emitted. The zigbee token reset is only reachable under the
 * zigbee_token_reset feature (build.rs defines OHF_ZIGBEE_TOKEN_RESET): it pulls zigbee stack
 * headers that don't exist in the non-zigbee RCP build. */
#include "sl_component_catalog.h"
#include "sl_status.h"
#include "sl_button.h"
#include "sl_simple_button.h"
#include "sl_sleeptimer.h"
#ifdef OHF_ZIGBEE_TOKEN_RESET
#include "stack/include/sl_zigbee.h"
#include "stack/include/stack-info.h"
#endif
#include "zbt2_reset_button_config.h"
