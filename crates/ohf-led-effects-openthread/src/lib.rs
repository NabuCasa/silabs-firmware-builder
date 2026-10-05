//! LED network state, polled from OpenThread.
#![no_std]

use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::sync::atomic::{AtomicBool, Ordering};

use ohf_sys::{sl_sleeptimer_start_periodic_timer_ms, sl_sleeptimer_timer_handle_t};

const SETTINGS_POLL_INTERVAL_MS: u32 = 250;

extern "C" {
    fn otGetInstance() -> *mut c_void;
    fn otLinkGetPanId(instance: *mut c_void) -> u16;
    fn otPlatRadioIsEnabled(instance: *mut c_void) -> bool;
    fn led_effects_init();
    fn led_effects_set_network_state(network_formed: bool);
}

struct SyncCell<T>(UnsafeCell<T>);
unsafe impl<T> Sync for SyncCell<T> {}
impl<T> SyncCell<T> {
    const fn new(v: T) -> Self {
        Self(UnsafeCell::new(v))
    }
    fn get(&self) -> *mut T {
        self.0.get()
    }
}

const ZERO_TIMER: sl_sleeptimer_timer_handle_t = sl_sleeptimer_timer_handle_t {
    callback_data: core::ptr::null_mut(),
    priority: 0,
    option_flags: 0,
    next: core::ptr::null_mut(),
    callback: None,
    timeout_periodic: 0,
    delta: 0,
    timeout_expected_tc: 0,
    conversion_error: 0,
    accumulated_error: 0,
};

static NETWORK_HAS_SETTINGS: AtomicBool = AtomicBool::new(false);
static POLL_TIMER: SyncCell<sl_sleeptimer_timer_handle_t> = SyncCell::new(ZERO_TIMER);

extern "C" fn network_state_poll_callback(
    _handle: *mut sl_sleeptimer_timer_handle_t,
    _data: *mut c_void,
) {
    unsafe {
        let instance = otGetInstance();
        let pan_id = otLinkGetPanId(instance);
        let has_network = pan_id != 0xFFFF && otPlatRadioIsEnabled(instance);

        if has_network != NETWORK_HAS_SETTINGS.load(Ordering::SeqCst) {
            NETWORK_HAS_SETTINGS.store(has_network, Ordering::SeqCst);
            led_effects_set_network_state(has_network);
        }
    }
}

#[no_mangle]
pub extern "C" fn device_has_stored_network_settings() -> bool {
    NETWORK_HAS_SETTINGS.load(Ordering::SeqCst)
}

#[no_mangle]
pub extern "C" fn led_effects_system_init() {
    unsafe {
        led_effects_init();
        led_effects_set_network_state(device_has_stored_network_settings());
        sl_sleeptimer_start_periodic_timer_ms(
            POLL_TIMER.get(),
            SETTINGS_POLL_INTERVAL_MS,
            Some(network_state_poll_callback),
            core::ptr::null_mut(),
            0,
            0,
        );
    }
}
