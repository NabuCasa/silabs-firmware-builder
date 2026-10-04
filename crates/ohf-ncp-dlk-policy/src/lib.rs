//! NCP dynamic link key policy — Rust port of `ncp_dlk_policy.c`.
//!
//! Overrides the stack's weak `sl_zigbee_zdo_dlk_select_negotiation_parameters_callback`.
//! The default commits the trust center to install-code DLK for any R23 joiner (they all
//! advertise install-code support) and never falls back to the R21 well-known-key network
//! key transport, so joins without a provisioned install code die. This policy only agrees
//! to negotiate when a link key is provisioned for the joiner's exact EUI64, and otherwise
//! returns an error — which the stack turns into the legacy key transport.
//!
//! The SDK's default is `SL_WEAK`; we linker-wrap it straight onto this Rust function
//! (`-Wl,--wrap=sl_zigbee_zdo_dlk_select_negotiation_parameters_callback` redirects the
//! stack's calls to `__wrap_…`, and the `--wrap` reference pulls it out of the aggregate
//! archive). lld honours the wrap over the LTO-visible weak default; GNU ld (PR ld/31956)
//! would call the weak default directly, so this path is LLVM-only.
#![no_std]
#![allow(non_camel_case_types, non_upper_case_globals)]

mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::*;

#[no_mangle]
pub unsafe extern "C" fn __wrap_sl_zigbee_zdo_dlk_select_negotiation_parameters_callback(
    partner: *const sl_zigbee_address_info,
    their_supported_methods: sl_zigbee_dlk_supported_negotiation_method,
    their_supported_secrets: sl_zigbee_dlk_negotiation_supported_shared_secret_source,
    selected_method: *mut sl_zigbee_dlk_negotiation_method,
    selected_secret: *mut sl_zigbee_dlk_negotiation_shared_secret_source,
) -> sl_status_t {
    if their_supported_secrets & DLK_SECRET_MASK_PRECONFIG_INSTALL_CODE as u8 == 0 {
        return SL_STATUS_NOT_SUPPORTED;
    }

    if their_supported_methods & DLK_PROTOCOL_MASK_SPEKE_C25519_SHA256 as u8 != 0 {
        *selected_method = DLK_PROTOCOL_ENUM_SPEKE_C25519_SHA256 as u8;
    } else if their_supported_methods & DLK_PROTOCOL_MASK_SPEKE_C25519_AES128 as u8 != 0 {
        *selected_method = DLK_PROTOCOL_ENUM_SPEKE_C25519_AES128 as u8;
    } else if their_supported_methods & DLK_PROTOCOL_MASK_STATIC_KEY_REQUEST as u8 != 0 {
        *selected_method = DLK_PROTOCOL_ENUM_STATIC_KEY as u8;
    } else {
        return SL_STATUS_NOT_SUPPORTED;
    }

    let mut context: sl_zigbee_sec_man_context_t = core::mem::zeroed();
    sl_zigbee_sec_man_init_context(&mut context);
    context.core_key_type = SL_ZB_SEC_MAN_KEY_TYPE_TC_LINK_WITH_TIMEOUT as _;
    context.flags |= ZB_SEC_MAN_FLAG_EUI_IS_VALID as u8;
    context.eui64 = (*partner).device_long;

    let mut metadata: sl_zigbee_sec_man_aps_key_metadata_t = core::mem::zeroed();
    if sl_zigbee_sec_man_get_aps_key_info(&mut context, &mut metadata) != SL_STATUS_OK {
        return SL_STATUS_NOT_FOUND;
    }

    *selected_secret = DLK_SECRET_ENUM_PRECONFIG_INSTALL_CODE as u8;
    SL_STATUS_OK
}
