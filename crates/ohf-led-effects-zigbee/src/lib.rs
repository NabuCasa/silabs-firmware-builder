//! Zigbee LED wrapper — Rust port of `led_effects_zigbee.c`.
//!
//! Translates Zigbee stack state into `led_effects_set_network_state`. The only part
//! that can't be Rust is reading the stack node-data token (`halCommonGetToken` is a
//! macro); that's a one-function SLC-compiled shim, so the logic stays here.
#![no_std]

extern "C" {
    /// SLC-compiled shim (led_effects_zigbee_shim.c): reads TOKEN_STACK_NODE_DATA.
    fn ohf_zigbee_stack_node_data(pan_id: *mut u16, channel: *mut u8);
    // From ohf-led-effects, resolved at the firmware link via the aggregate.
    fn led_effects_init();
    fn led_effects_set_network_state(network_formed: bool);
}

#[no_mangle]
pub extern "C" fn device_has_stored_network_settings() -> bool {
    let mut pan_id: u16 = 0;
    let mut channel: u8 = 0;
    unsafe { ohf_zigbee_stack_node_data(&mut pan_id, &mut channel) };

    pan_id != 0xFFFF && (11..=26).contains(&channel)
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
