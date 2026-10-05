//! ZBT-2 pin-hole reset button. Holding it blinks 1, 2, … times, and completing
//! `ZBT2_RESET_BUTTON_CYCLES` cycles resets the adapter. Releasing early cancels.
#![no_std]

use core::cell::{RefCell, UnsafeCell};
use core::ffi::c_void;
use core::mem::MaybeUninit;

use critical_section::Mutex;
use ohf_led_effects::{
    led_manager_clear_pattern, led_manager_set_color, LED_COLOR_RESET_ORANGE, LED_PRIORITY_CRITICAL,
};

#[allow(non_camel_case_types, non_upper_case_globals, non_snake_case, dead_code)]
mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::*;

// From the project's autogen
extern "C" {
    static sl_button_pin_hole_button: sl_button_t;
}

struct State {
    reset_cycle: u8,
    blink_count: u8,
    led_on: bool,
}

static STATE: Mutex<RefCell<State>> = Mutex::new(RefCell::new(State {
    reset_cycle: 0,
    blink_count: 0,
    led_on: false,
}));

// Sleeptimer handles, which the SDK writes to
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

const ZERO_TIMER: sl_sleeptimer_timer_handle_t =
    unsafe { MaybeUninit::zeroed().assume_init() };
static RESET_TIMER: SyncCell<sl_sleeptimer_timer_handle_t> = SyncCell::new(ZERO_TIMER);
static BLINK_TIMER: SyncCell<sl_sleeptimer_timer_handle_t> = SyncCell::new(ZERO_TIMER);

unsafe fn start_ms(
    timer: &SyncCell<sl_sleeptimer_timer_handle_t>,
    timeout_ms: u32,
    callback: sl_sleeptimer_timer_callback_t,
) {
    let mut ticks: u32 = 0;
    sl_sleeptimer_ms32_to_tick(timeout_ms, &mut ticks);
    sl_sleeptimer_start_timer(timer.get(), ticks, callback, core::ptr::null_mut(), 0, 0);
}

unsafe fn reset_adapter() {
    #[cfg(feature = "zigbee_token_reset")]
    {
        led_manager_set_color(LED_PRIORITY_CRITICAL, LED_COLOR_RESET_ORANGE);
        // Keeps the frame counters and boot counter
        sl_zigbee_token_factory_reset(true, true);
        ohf_sys::ohf_system_reset();
    }
    #[cfg(not(feature = "zigbee_token_reset"))]
    {
        led_manager_set_color(LED_PRIORITY_CRITICAL, ohf_led_effects::LED_COLOR_RESET_RED);
        ohf_sys::factory_erase();
    }
}

unsafe extern "C" fn reset_timer_callback(_handle: *mut sl_sleeptimer_timer_handle_t, _data: *mut c_void) {
    critical_section::with(|cs| STATE.borrow(cs).borrow_mut().reset_cycle += 1);
    start_ms(&BLINK_TIMER, ZBT2_RESET_BUTTON_BLINK_START_DELAY_MS, Some(blink_task));
}

unsafe extern "C" fn blink_task(_handle: *mut sl_sleeptimer_timer_handle_t, _data: *mut c_void) {
    let led_on = critical_section::with(|cs| STATE.borrow(cs).borrow().led_on);

    if led_on {
        // Reveal the layer below
        led_manager_clear_pattern(LED_PRIORITY_CRITICAL);

        let cycle_done = critical_section::with(|cs| {
            let mut s = STATE.borrow(cs).borrow_mut();
            s.led_on = false;
            if s.blink_count >= s.reset_cycle {
                s.blink_count = 0;
                Some(s.reset_cycle == ZBT2_RESET_BUTTON_CYCLES as u8)
            } else {
                None
            }
        });

        match cycle_done {
            Some(true) => reset_adapter(),
            Some(false) => start_ms(&RESET_TIMER, ZBT2_RESET_BUTTON_CYCLE_DELAY_MS, Some(reset_timer_callback)),
            None => start_ms(&BLINK_TIMER, ZBT2_RESET_BUTTON_BLINK_OFF_MS, Some(blink_task)),
        }
    } else {
        led_manager_set_color(LED_PRIORITY_CRITICAL, LED_COLOR_RESET_ORANGE);
        critical_section::with(|cs| {
            let mut s = STATE.borrow(cs).borrow_mut();
            s.led_on = true;
            s.blink_count += 1;
        });
        start_ms(&BLINK_TIMER, ZBT2_RESET_BUTTON_BLINK_ON_MS, Some(blink_task));
    }
}

unsafe fn handle_state(pressed: bool) {
    sl_sleeptimer_stop_timer(RESET_TIMER.get());
    sl_sleeptimer_stop_timer(BLINK_TIMER.get());

    if pressed {
        critical_section::with(|cs| {
            let mut s = STATE.borrow(cs).borrow_mut();
            s.reset_cycle = 0;
            s.blink_count = 0;
            s.led_on = false;
        });
        start_ms(&RESET_TIMER, ZBT2_RESET_BUTTON_CYCLE_DELAY_MS, Some(reset_timer_callback));
    } else {
        // Released early
        led_manager_clear_pattern(LED_PRIORITY_CRITICAL);
        critical_section::with(|cs| STATE.borrow(cs).borrow_mut().led_on = false);
    }
}

// Wraps the SDK's weak sl_button_on_change
#[no_mangle]
pub unsafe extern "C" fn __wrap_sl_button_on_change(handle: *const sl_button_t) {
    if handle == core::ptr::addr_of!(sl_button_pin_hole_button) {
        let pressed = sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED as sl_button_state_t;
        handle_state(pressed);
    }
}
