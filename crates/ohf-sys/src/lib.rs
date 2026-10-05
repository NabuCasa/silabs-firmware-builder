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
    /// Only present in zigbee builds; callers (ohf-led-effects-zigbee, ohf-router) are
    /// zigbee-only too.
    pub fn ohf_zigbee_stack_node_data(pan_id: *mut u16, channel: *mut u8, node_type: *mut u8);

    /// Wrapper over the inline, no-return `NVIC_SystemReset` (defined in shims.c).
    pub fn ohf_system_reset() -> !;
}

/// Factory-erase and reboot: full NVM3 erase + PSA key wipe, then reset. Shared by the reset
/// button and the router's nvram reset. Gated behind the `factory_erase` feature so firmwares
/// that don't need it (e.g. Z-Wave) don't pull the nvm3/psa surface.
#[cfg(feature = "factory_erase")]
pub fn factory_erase() -> ! {
    // The Zigbee stack's PSA key id range.
    const ZB_PSA_KEY_ID_MIN: u32 = 0x0003_0000;
    const ZB_PSA_KEY_ID_MAX: u32 = 0x0003_FFFF;
    unsafe {
        nvm3_initDefault();
        nvm3_eraseAll(nvm3_defaultHandle);
        let mut key_id = ZB_PSA_KEY_ID_MIN;
        while key_id <= ZB_PSA_KEY_ID_MAX {
            psa_destroy_key(key_id);
            key_id += 1;
        }
        ohf_system_reset()
    }
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
