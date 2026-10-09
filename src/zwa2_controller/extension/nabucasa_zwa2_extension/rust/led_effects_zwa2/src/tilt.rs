//! The tilt monitor: blinks the LED while the device is tilted off vertical.
use core::cell::RefCell;
use core::ffi::c_void;

use critical_section::Mutex;
use ohf_sys::{
    sl_sleeptimer_start_periodic_timer_ms, sl_sleeptimer_timer_handle_t, SyncCell, ZERO_TIMER,
};

use crate::manager::{self, color, Pattern, Priority, UPDATE_INTERVAL_MS};

// sin² of the angle from vertical that counts as tilted (16°) and, with 4° of
// hysteresis, of the angle below which it counts as level again (12°)
const TILT_THRESHOLD_SIN2: f32 = 0.075975955;
const TILT_RELEASE_SIN2: f32 = 0.04322727;

// A reading is stable when every axis is within this raw delta of the previous one, and a
// tilt decision needs this many consecutive stable readings
const STABLE_DELTA: u16 = 25;
const STABLE_COUNT: u32 = 3;

const SAMPLE_MS: u32 = 100;
const SAMPLE_TICKS: u32 = SAMPLE_MS / UPDATE_INTERVAL_MS;

const BLINK_PERIOD_MS: u16 = 500;

struct Tilt {
    monitoring: bool,
    enabled: bool,
    tilted: bool,
    ticks: u32,
    last_reading: [i16; 3],
    stable_count: u32,
}

// Also used from the sleeptimer ISR
static TILT: Mutex<RefCell<Tilt>> = Mutex::new(RefCell::new(Tilt {
    monitoring: false,
    enabled: true,
    tilted: false,
    ticks: 0,
    last_reading: [0; 3],
    stable_count: 0,
}));
static TIMER: SyncCell<sl_sleeptimer_timer_handle_t> = SyncCell::new(ZERO_TIMER);

// `asin(|horizontal| / |total|) > threshold`, compared as squared sines to avoid the
// trigonometry. Both sides are monotonic over 0-90 degrees. Upright and upside-down both
// count as vertical. `None` when the reading is too small to judge.
fn is_tilted(reading: [i16; 3], threshold_sin2: f32) -> Option<bool> {
    let [x, y, z] = reading.map(f32::from);
    let horizontal = x * x + y * y;
    let total = horizontal + z * z;
    (total >= 1.0).then(|| horizontal > threshold_sin2 * total)
}

fn is_stable(reading: [i16; 3], last: [i16; 3]) -> bool {
    reading
        .iter()
        .zip(&last)
        .all(|(a, b)| a.wrapping_sub(*b).unsigned_abs() < STABLE_DELTA)
}

extern "C" fn monitor_callback(_handle: *mut sl_sleeptimer_timer_handle_t, _data: *mut c_void) {
    let sample_due = critical_section::with(|cs| {
        let mut tilt = TILT.borrow(cs).borrow_mut();
        tilt.ticks = tilt.ticks.wrapping_add(1);
        if !tilt.enabled {
            if core::mem::take(&mut tilt.tilted) {
                manager::clear_pattern(Priority::Tilt);
            }
            return false;
        }
        tilt.ticks % SAMPLE_TICKS == 0
    });
    if !sample_due {
        return;
    }

    // Outside the critical section, since I2C takes a while
    let reading = qma6100p_driver::read_raw();

    critical_section::with(|cs| {
        let mut tilt = TILT.borrow(cs).borrow_mut();

        let stable = is_stable(reading, tilt.last_reading);
        tilt.last_reading = reading;
        if stable {
            tilt.stable_count = tilt.stable_count.saturating_add(1);
        } else {
            tilt.stable_count = 0;
            return;
        }
        if tilt.stable_count < STABLE_COUNT {
            return;
        }

        let threshold = if tilt.tilted {
            TILT_RELEASE_SIN2
        } else {
            TILT_THRESHOLD_SIN2
        };
        let Some(tilted) = is_tilted(reading, threshold) else {
            return;
        };
        if tilted == tilt.tilted {
            return;
        }
        tilt.tilted = tilted;
        if tilted {
            manager::set_pattern(
                Priority::Tilt,
                Pattern::blink(color::COLD_WHITE, BLINK_PERIOD_MS),
            );
        } else {
            manager::clear_pattern(Priority::Tilt);
        }
    });
}

pub fn start() {
    let already = critical_section::with(|cs| {
        let mut tilt = TILT.borrow(cs).borrow_mut();
        if tilt.monitoring {
            return true;
        }
        tilt.monitoring = true;
        tilt.ticks = 0;
        tilt.tilted = false;
        false
    });
    if already {
        return;
    }
    unsafe {
        sl_sleeptimer_start_periodic_timer_ms(
            TIMER.get(),
            UPDATE_INTERVAL_MS,
            Some(monitor_callback),
            core::ptr::null_mut(),
            0,
            0,
        );
    }
}

pub fn set_enabled(enabled: bool) {
    critical_section::with(|cs| {
        let mut tilt = TILT.borrow(cs).borrow_mut();
        tilt.enabled = enabled;
        // Disabling while tilted clears the blink immediately
        if !enabled && core::mem::take(&mut tilt.tilted) {
            manager::clear_pattern(Priority::Tilt);
        }
    });
}
