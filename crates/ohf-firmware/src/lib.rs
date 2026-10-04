//! Aggregate of the Rust-backed firmware components into one staticlib.
//!
//! Component crates are `no_std` rlibs with no panic handler; this crate links them
//! and provides the single `#[panic_handler]`, so `rust_begin_unwind` and `core`
//! appear exactly once in the final link. The firmware's linker gc-sections drops any
//! component whose C-ABI exports go unreferenced for a given config.
#![no_std]

// Pull each enabled component in so its #[no_mangle] exports land in the staticlib.
// Features are selected by SLC (see Cargo.toml).
#[cfg(feature = "led_effects")]
extern crate ohf_led_effects;
#[cfg(feature = "led_effects_zigbee")]
extern crate ohf_led_effects_zigbee;
#[cfg(feature = "qma6100p")]
extern crate ohf_qma6100p;
#[cfg(feature = "ws2812")]
extern crate ohf_ws2812;

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    // TODO: route into the SDK crash handler (ot_crash_handler / RESET_*).
    loop {}
}
