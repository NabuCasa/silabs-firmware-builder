/*
 * Strong override for the stack's SL_WEAK sl_zigbee_zdo_dlk_select_negotiation_parameters_callback
 * (default in sl_zigbee_r23_app_stubs.c), delegating to the Rust policy
 * (crates/ohf-ncp-dlk-policy). As a direct source object it always wins over the weak
 * default; a strong override living in the Rust aggregate archive would not be pulled
 * under -flto -fwhole-program.
 */
#include PLATFORM_HEADER
#include "sl_zigbee.h"
#include "stack/include/sl_zigbee_zdo_dlk_negotiation.h"

extern sl_status_t ohf_ncp_dlk_select_negotiation_parameters(
  const sl_zigbee_address_info *partner,
  sl_zigbee_dlk_supported_negotiation_method their_supported_methods,
  sl_zigbee_dlk_negotiation_supported_shared_secret_source their_supported_secrets,
  sl_zigbee_dlk_negotiation_method *selected_method,
  sl_zigbee_dlk_negotiation_shared_secret_source *selected_secret);

sl_status_t sl_zigbee_zdo_dlk_select_negotiation_parameters_callback(
  sl_zigbee_address_info *partner,
  sl_zigbee_dlk_supported_negotiation_method their_supported_methods,
  sl_zigbee_dlk_negotiation_supported_shared_secret_source their_supported_secrets,
  sl_zigbee_dlk_negotiation_method *selected_method,
  sl_zigbee_dlk_negotiation_shared_secret_source *selected_secret)
{
  return ohf_ncp_dlk_select_negotiation_parameters(partner, their_supported_methods,
                                                   their_supported_secrets, selected_method,
                                                   selected_secret);
}
