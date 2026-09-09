/*
 * ezsp_version.c
 *
 * Replaces the stack library's `sl_zigbee_version` so the EZSP patch number override
 * is applied at compile time. Providing the definition here keeps the linker from
 * pulling `sl_zigbee_version.c.obj` out of the stack archive, whose value LTO would
 * otherwise fold into the `getValue(VERSION_INFO)` handler.
 */

#include "sl_zigbee.h"
#include "xncp_config.h"

#if XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE == 0xFF
  #define XNCP_EZSP_SPECIAL_VERSION SL_ZIGBEE_SPECIAL_VERSION
#else
  #define XNCP_EZSP_SPECIAL_VERSION XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE
#endif

const sl_zigbee_version_t sl_zigbee_version = {
  .build = SL_ZIGBEE_BUILD_NUMBER,
  .major = SL_ZIGBEE_MAJOR_VERSION,
  .minor = SL_ZIGBEE_MINOR_VERSION,
  .patch = SL_ZIGBEE_PATCH_VERSION,
  .special = XNCP_EZSP_SPECIAL_VERSION,
  .type = SL_ZIGBEE_VERSION_TYPE,
};
