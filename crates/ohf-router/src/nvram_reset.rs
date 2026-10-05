//! Rust port of `router_nvram_reset.c` — if the stored node data looks like a leftover
//! coordinator/NCP (has a network but a non-router node type), factory-erase so the device
//! starts clean as a router.
use ohf_sys::{factory_erase, ohf_zigbee_stack_node_data};

use crate::bindings::{SL_ZIGBEE_ROUTER, SL_ZIGBEE_UNKNOWN_DEVICE};

#[no_mangle]
pub extern "C" fn router_nvram_reset_init(_init_level: u8) {
    let mut pan_id = 0u16;
    let mut channel = 0u8;
    let mut node_type = 0u8;
    unsafe { ohf_zigbee_stack_node_data(&mut pan_id, &mut channel, &mut node_type) };

    if pan_id != 0xFFFF
        && node_type != SL_ZIGBEE_ROUTER as u8
        && node_type != SL_ZIGBEE_UNKNOWN_DEVICE as u8
    {
        factory_erase();
    }
}
