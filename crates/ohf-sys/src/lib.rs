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

    /// Reads TOKEN_STACK_NODE_DATA via the `halCommonGetToken` macro (defined in shims.c).
    /// Only present in zigbee builds; callers (ohf-led-effects-zigbee) are zigbee-only too.
    pub fn ohf_zigbee_stack_node_data(pan_id: *mut u16, channel: *mut u8);

    /// Wrapper over the inline, no-return `NVIC_SystemReset` (defined in shims.c).
    pub fn ohf_system_reset() -> !;
}

// The critical-section impl, backed by the SDK's CORE_Enter/ExitCritical. Registering
// it here lets any component use `critical_section::with` and `critical_section::Mutex`
// for shared state the compiler only lets you touch while interrupts are masked.
struct SdkCriticalSection;
critical_section::set_impl!(SdkCriticalSection);

unsafe impl critical_section::Impl for SdkCriticalSection {
    unsafe fn acquire() -> critical_section::RawRestoreState {
        CORE_EnterCritical()
    }

    unsafe fn release(state: critical_section::RawRestoreState) {
        CORE_ExitCritical(state);
    }
}
