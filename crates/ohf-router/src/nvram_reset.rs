//! Rust port of `router_nvram_reset.c` — if the stored node data looks like a leftover
//! coordinator/NCP (has a network but a non-router node type), factory-erase so the device
//! starts clean as a router.
use ohf_sys::factory_erase;

use crate::bindings::{SL_ZIGBEE_ROUTER, SL_ZIGBEE_UNKNOWN_DEVICE};

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
