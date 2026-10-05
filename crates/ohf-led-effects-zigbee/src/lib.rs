//! Zigbee LED wrapper — Rust port of `led_effects_zigbee.c`.
//!
//! Translates Zigbee stack state into `led_effects_set_network_state`, reading the stack
//! node-data token through `ohf_sys::token`.
#![no_std]

extern "C" {
    // From ohf-led-effects, resolved at the firmware link via the aggregate.
    fn led_effects_init();
    fn led_effects_set_network_state(network_formed: bool);
}

#[no_mangle]
pub extern "C" fn device_has_stored_network_settings() -> bool {
    let node: ohf_sys::tokTypeStackNodeData = ohf_sys::token::get(ohf_sys::TOKEN_STACK_NODE_DATA);
    node.panId != 0xFFFF && (11..=26).contains(&node.radioFreqChannel)
}

#[no_mangle]
pub extern "C" fn led_effects_system_init(_init_level: u8) {
    unsafe {
        led_effects_init();
        led_effects_set_network_state(device_has_stored_network_settings());
    }
}

#[no_mangle]
pub extern "C" fn led_effects_stack_status_callback(_status: u32) {
    unsafe { led_effects_set_network_state(device_has_stored_network_settings()) }
}
