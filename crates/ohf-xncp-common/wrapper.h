/* The SDK + config surface this command set binds. The allowlist in build.rs controls
 * what is actually emitted; these includes just make the declarations visible.
 *
 * xncp_config.h is the component's config_file: SLC patches the manifest overrides into
 * it and leaves the header defaults otherwise, so bindgen sees the same resolved values
 * the C build does — one source of truth for both languages. */
#include "sl_component_catalog.h"
#include "sl_status.h"
#include "stack/include/sl_zigbee.h"
#include "stack/include/sl_zigbee_types.h"
#include "stack/include/sl_zigbee_types_internal.h"
#include "stack/include/message.h"
#include "stack/include/stack-info.h"
#include "stack/include/sl_zigbee_random_api.h"
#include "ezsp-enum.h"
#include "em_usart.h"
#include "em_device.h"
#include "xncp_config.h"
