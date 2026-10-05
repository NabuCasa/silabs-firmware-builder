//! Invert the upper EUI64 octets, so a former coordinator gets a new address as a router.
use core::ffi::c_void;

use crate::bindings::{
    sl_status_t, SL_STATUS_OK, SL_TOKEN_STATIC_TOKEN_SIZE_OFFSET, SL_TOKEN_TYPE_STATIC_DEVICE,
    TOKEN_MFG_EUI_64, TOKEN_MFG_EUI_64_SIZE,
};

// SL_TOKEN_GET_STATIC_DEVICE_TOKEN(TOKEN_MFG_EUI_64): type | key | (size << offset).
const MFG_EUI64_TOKEN: u32 = SL_TOKEN_TYPE_STATIC_DEVICE as u32
    | TOKEN_MFG_EUI_64 as u32
    | ((TOKEN_MFG_EUI_64_SIZE as u32) << SL_TOKEN_STATIC_TOKEN_SIZE_OFFSET as u32);

extern "C" {
    fn __real_sl_token_manager_get_data(token: u32, data: *mut c_void, length: u32) -> sl_status_t;
}

#[no_mangle]
pub unsafe extern "C" fn __wrap_sl_token_manager_get_data(
    token: u32,
    data: *mut c_void,
    length: u32,
) -> sl_status_t {
    let status = __real_sl_token_manager_get_data(token, data, length);

    if token == MFG_EUI64_TOKEN && status == SL_STATUS_OK {
        // EUI64 is stored little-endian; invert the upper 6 octets.
        for b in core::slice::from_raw_parts_mut(data as *mut u8, 6) {
            *b ^= 0xFF;
        }
    }
    status
}
