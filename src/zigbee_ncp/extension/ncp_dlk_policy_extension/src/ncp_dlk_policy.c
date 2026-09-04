#include PLATFORM_HEADER
#include "sl_zigbee.h"
#include "sl_zigbee_debug_print.h"
#include "stack/include/sl_zigbee_zdo_dlk_negotiation.h"
#include "stack/include/zigbee-security-manager.h"

// Strong override of the weak default in `sl_zigbee_r23_app_stubs.c`.
//
// Returning an error here makes the trust center skip DLK and deliver the network
// key the R21 way, encrypted with the well-known key. Returning SL_STATUS_OK commits
// the trust center to a negotiation with no fallback: it ignores the joiner's error
// response and never sends the network key. So we only proceed when the host has
// provisioned a link key for this specific joiner, which is the install code flow.
//
// `sl_zigbee_sec_man_export_transient_key_by_eui` matches the exact EUI64 only. The
// wildcard FF:FF:FF:FF:FF:FF:FF:FF entry that bellows and network-creator-security add
// on permit-join does not match here, which is the whole point.
sl_status_t sl_zigbee_zdo_dlk_select_negotiation_parameters_callback(
  sl_zigbee_address_info *partner,
  sl_zigbee_dlk_supported_negotiation_method their_supported_methods,
  sl_zigbee_dlk_negotiation_supported_shared_secret_source their_supported_secrets,
  sl_zigbee_dlk_negotiation_method *selected_method,
  sl_zigbee_dlk_negotiation_shared_secret_source *selected_secret)
{
  sl_zigbee_sec_man_context_t context;
  sl_zigbee_sec_man_key_t key;
  sl_zigbee_sec_man_aps_key_metadata_t metadata;

  if (!(their_supported_secrets & DLK_SECRET_MASK_PRECONFIG_INSTALL_CODE)) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  if (their_supported_methods & DLK_PROTOCOL_MASK_SPEKE_C25519_SHA256) {
    *selected_method = DLK_PROTOCOL_ENUM_SPEKE_C25519_SHA256;
  } else if (their_supported_methods & DLK_PROTOCOL_MASK_SPEKE_C25519_AES128) {
    *selected_method = DLK_PROTOCOL_ENUM_SPEKE_C25519_AES128;
  } else {
    return SL_STATUS_NOT_SUPPORTED;
  }

  sl_status_t status = sl_zigbee_sec_man_export_transient_key_by_eui(
    partner->device_long, &context, &key, &metadata);

  if (status != SL_STATUS_OK) {
    sl_zigbee_app_debug_print("DLK: no link key for ");
    sl_zigbee_app_debug_print_buffer(partner->device_long, EUI64_SIZE, false);
    sl_zigbee_app_debug_println(", using legacy join");
    return SL_STATUS_NOT_FOUND;
  }

  *selected_secret = DLK_SECRET_ENUM_PRECONFIG_INSTALL_CODE;
  return SL_STATUS_OK;
}
