//! Aggregate of the Rust-backed firmware components into one staticlib.
//!
//! Component crates are `no_std` rlibs with no panic handler; this crate links them
//! and provides the single `#[panic_handler]`, so `rust_begin_unwind` and `core`
//! appear exactly once in the final link. The firmware's linker gc-sections drops any
//! component whose C-ABI exports go unreferenced for a given config.
#![no_std]

// Pull each component in so its #[no_mangle] exports land in the staticlib.
extern crate ohf_qma6100p;
extern crate ohf_ws2812;

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    // TODO: route into the SDK crash handler (ot_crash_handler / RESET_*).
    loop {}
}
