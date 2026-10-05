//! Runtime support linked into every Rust-backed firmware.
#![no_std]
#![allow(non_camel_case_types, non_upper_case_globals, non_snake_case, dead_code)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

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
