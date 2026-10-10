//! Negotiate DLK only when a link key is provisioned for the joiner's EUI64. Otherwise the
//! stack falls back to the R21 network key transport. The SDK default commits every R23
//! joiner to install-code DLK, so joins without an install code fail.
#![no_std]

#[allow(
    non_camel_case_types,
    non_upper_case_globals,
    non_snake_case,
    dead_code
)]
mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::{
    sl_status_t, sl_zigbee_address_info, sl_zigbee_dlk_negotiation_method,
    sl_zigbee_dlk_negotiation_shared_secret_source,
    sl_zigbee_dlk_negotiation_supported_shared_secret_source,
    sl_zigbee_dlk_supported_negotiation_method, sl_zigbee_sec_man_aps_key_metadata_t,
    sl_zigbee_sec_man_context_t, sl_zigbee_sec_man_get_aps_key_info,
    sl_zigbee_sec_man_init_context, DLK_PROTOCOL_ENUM_SPEKE_C25519_AES128,
    DLK_PROTOCOL_ENUM_SPEKE_C25519_SHA256, DLK_PROTOCOL_ENUM_STATIC_KEY,
    DLK_PROTOCOL_MASK_SPEKE_C25519_AES128, DLK_PROTOCOL_MASK_SPEKE_C25519_SHA256,
    DLK_PROTOCOL_MASK_STATIC_KEY_REQUEST, DLK_SECRET_ENUM_PRECONFIG_INSTALL_CODE,
    DLK_SECRET_MASK_PRECONFIG_INSTALL_CODE, SL_STATUS_NOT_FOUND, SL_STATUS_NOT_SUPPORTED,
    SL_STATUS_OK, SL_ZB_SEC_MAN_KEY_TYPE_TC_LINK_WITH_TIMEOUT, ZB_SEC_MAN_FLAG_EUI_IS_VALID,
};

type Selection = (
    sl_zigbee_dlk_negotiation_method,
    sl_zigbee_dlk_negotiation_shared_secret_source,
);

fn select(
    partner: &sl_zigbee_address_info,
    their_supported_methods: sl_zigbee_dlk_supported_negotiation_method,
    their_supported_secrets: sl_zigbee_dlk_negotiation_supported_shared_secret_source,
) -> Result<Selection, sl_status_t> {
    if their_supported_secrets & DLK_SECRET_MASK_PRECONFIG_INSTALL_CODE as u8 == 0 {
        return Err(SL_STATUS_NOT_SUPPORTED);
    }

    let method = if their_supported_methods & DLK_PROTOCOL_MASK_SPEKE_C25519_SHA256 as u8 != 0 {
        DLK_PROTOCOL_ENUM_SPEKE_C25519_SHA256
    } else if their_supported_methods & DLK_PROTOCOL_MASK_SPEKE_C25519_AES128 as u8 != 0 {
        DLK_PROTOCOL_ENUM_SPEKE_C25519_AES128
    } else if their_supported_methods & DLK_PROTOCOL_MASK_STATIC_KEY_REQUEST as u8 != 0 {
        DLK_PROTOCOL_ENUM_STATIC_KEY
    } else {
        return Err(SL_STATUS_NOT_SUPPORTED);
    };

    // Only with a link key provisioned for the joiner
    let mut context: sl_zigbee_sec_man_context_t = unsafe { core::mem::zeroed() };
    unsafe { sl_zigbee_sec_man_init_context(&mut context) };
    context.core_key_type = SL_ZB_SEC_MAN_KEY_TYPE_TC_LINK_WITH_TIMEOUT as _;
    context.flags |= ZB_SEC_MAN_FLAG_EUI_IS_VALID as u8;
    context.eui64 = partner.device_long;

    let mut metadata: sl_zigbee_sec_man_aps_key_metadata_t = unsafe { core::mem::zeroed() };
    if unsafe { sl_zigbee_sec_man_get_aps_key_info(&mut context, &mut metadata) } != SL_STATUS_OK {
        return Err(SL_STATUS_NOT_FOUND);
    }

    Ok((method as _, DLK_SECRET_ENUM_PRECONFIG_INSTALL_CODE as _))
}

// Overrides the SDK's weak default
#[no_mangle]
pub extern "C" fn sl_zigbee_zdo_dlk_select_negotiation_parameters_callback(
    partner: *const sl_zigbee_address_info,
    their_supported_methods: sl_zigbee_dlk_supported_negotiation_method,
    their_supported_secrets: sl_zigbee_dlk_negotiation_supported_shared_secret_source,
    selected_method: *mut sl_zigbee_dlk_negotiation_method,
    selected_secret: *mut sl_zigbee_dlk_negotiation_shared_secret_source,
) -> sl_status_t {
    let partner = unsafe { &*partner };
    match select(partner, their_supported_methods, their_supported_secrets) {
        Ok((method, secret)) => {
            unsafe {
                *selected_method = method;
                *selected_secret = secret;
            }
            SL_STATUS_OK
        }
        Err(status) => status,
    }
}
