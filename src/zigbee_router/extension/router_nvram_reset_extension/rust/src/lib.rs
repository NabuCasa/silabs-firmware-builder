//! Erase leftover coordinator network data, so the device starts clean as a router.
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

use ohf_sys::factory_erase;

use bindings::{SL_ZIGBEE_ROUTER, SL_ZIGBEE_UNKNOWN_DEVICE};

#[no_mangle]
pub extern "C" fn router_nvram_reset_init(_init_level: u8) {
    let node: ohf_sys::tokTypeStackNodeData = ohf_sys::token::get(ohf_sys::TOKEN_STACK_NODE_DATA);

    if node.panId != 0xFFFF
        && node.nodeType != SL_ZIGBEE_ROUTER as u8
        && node.nodeType != SL_ZIGBEE_UNKNOWN_DEVICE as u8
    {
        factory_erase();
    }
}
