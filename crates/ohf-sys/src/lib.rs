//! FFI to the Silicon Labs SDK.
//!
//! Every type, constant, and (real, non-inline) function declaration is
//! bindgen-generated at build time from the SDK headers, using the include/define
//! flags SLC emitted (`OHF_BINDGEN_FLAGS`, supplied by `build_project.py`). We
//! hand-pick only the *surface* — the `#include`s in `wrapper.h` and the allowlist
//! in `build.rs` — never the layouts. Inline/macro-only SDK helpers can't be bound
//! and get a thin C shim in the owning component instead.
#![no_std]
#![allow(non_camel_case_types, non_upper_case_globals, non_snake_case, dead_code)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

extern "C" {
    /// Generic wrapper over the inline `GPIO_PinModeSet` (defined in shims.c, compiled
    /// into this crate). `mode` is a `gpioMode*` value.
    pub fn ohf_gpio_pin_mode_set(port: u32, pin: u32, mode: u32, out: u32);
}
