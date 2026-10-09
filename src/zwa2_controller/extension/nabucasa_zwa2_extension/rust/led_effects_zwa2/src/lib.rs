//! The ZWA-2's LED behaviours: a priority-layered LED manager, the tilt monitor and the
//! host connection state.
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

mod manager;
mod nvm;
mod tilt;

pub use manager::{clear_pattern, color, set_color, set_pattern, Pattern, Priority, Rgb};
pub use nvm::{config_flag, config_set, store_led, ConfigKey};
pub use tilt::set_enabled as set_tilt_enabled;

// The host search pulse
const SEARCH_PULSE_PERIOD_MS: u16 = 2760;
const SEARCH_PULSE_MIN_BRIGHTNESS: u16 = 6554; // ~10%

/// Searching for the host: pulse white on the background layer
pub fn set_searching() {
    set_pattern(
        Priority::Background,
        Pattern::pulse(
            color::COLD_WHITE,
            SEARCH_PULSE_PERIOD_MS,
            SEARCH_PULSE_MIN_BRIGHTNESS,
            65535,
        ),
    );
}

/// Host connected: solid white on the background layer
pub fn set_connected() {
    set_color(Priority::Background, color::COLD_WHITE);
}

/// Starts the LED manager and the tilt monitor, restores the host-set LED state and
/// configuration, and shows the search pulse. Called from the application's init.
#[no_mangle]
pub extern "C" fn led_effects_zwa2_init() {
    manager::init();
    // Tilt monitoring runs regardless of the network state
    tilt::start();

    if let Some(color) = nvm::restore_led() {
        set_color(Priority::Manual, color);
    }
    tilt::set_enabled(config_flag(ConfigKey::EnableTiltIndicator as u8));

    set_searching();
}

/// Called on the first Serial API command from the host
#[no_mangle]
pub extern "C" fn led_effects_zwa2_host_connected() {
    set_connected();
}
