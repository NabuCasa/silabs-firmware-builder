//! LED network state, polled from OpenThread.
#![no_std]

use core::ffi::c_void;
use core::sync::atomic::{AtomicBool, Ordering};

use ohf_sys::{sl_sleeptimer_start_periodic_timer_ms, sl_sleeptimer_timer_handle_t, SyncCell, ZERO_TIMER};

const SETTINGS_POLL_INTERVAL_MS: u32 = 250;

extern "C" {
    fn otGetInstance() -> *mut c_void;
    fn otLinkGetPanId(instance: *mut c_void) -> u16;
    fn otPlatRadioIsEnabled(instance: *mut c_void) -> bool;
}

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

        if has_network != NETWORK_HAS_SETTINGS.swap(has_network, Ordering::SeqCst) {
            ohf_led_effects::set_network_state(has_network);
        }
    }
}

#[no_mangle]
pub extern "C" fn led_effects_system_init() {
    ohf_led_effects::init();
    ohf_led_effects::set_network_state(NETWORK_HAS_SETTINGS.load(Ordering::SeqCst));
    unsafe {
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
