//! LED network state from the Zigbee stack.
#![no_std]

use ohf_sys::token::has_stored_network;

#[no_mangle]
pub extern "C" fn led_effects_system_init(_init_level: u8) {
    ohf_led_effects::init();
    ohf_led_effects::set_network_state(has_stored_network());
}

#[no_mangle]
pub extern "C" fn led_effects_stack_status_callback(_status: u32) {
    ohf_led_effects::set_network_state(has_stored_network());
}
