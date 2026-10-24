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

// `asin(|horizontal| / |total|) > threshold`, compared as squared sines to avoid the
// trigonometry. Both sides are monotonic over 0-90 degrees. Upright and upside-down both
// count as vertical. `None` when the reading is too small to judge.
fn is_tilted(reading: [i16; 3], threshold_sin2: f32) -> Option<bool> {
    let [x, y, z] = reading.map(f32::from);
    let horizontal = x * x + y * y;
    let total = horizontal + z * z;
    (total >= 1.0).then(|| horizontal > threshold_sin2 * total)
}

/// Counts consecutive readings that stay within `STABLE_DELTA` of the previous one
struct Debounce {
    last: [i16; 3],
    stable: u32,
}

impl Debounce {
    const NEW: Self = Self {
        last: [0; 3],
        stable: 0,
    };

    /// Whether `reading` is at least the `STABLE_COUNT`th stable one in a row
    fn settled(&mut self, reading: [i16; 3]) -> bool {
        let stable = reading
            .iter()
            .zip(&self.last)
            .all(|(a, b)| a.wrapping_sub(*b).unsigned_abs() < STABLE_DELTA);
        self.last = reading;
        self.stable = if stable {
            self.stable.saturating_add(1)
        } else {
            0
        };
        self.stable >= STABLE_COUNT
    }
}

struct Tilt {
    monitoring: bool,
    enabled: bool,
    tilted: bool,
    ticks: u32,
    debounce: Debounce,
}

impl Tilt {
    /// Counts a timer tick; whether an accelerometer sample is due
    fn tick(&mut self) -> bool {
        self.ticks = self.ticks.wrapping_add(1);
        if !self.enabled {
            self.set_tilted(false);
            return false;
        }
        self.ticks % SAMPLE_TICKS == 0
    }

    fn sample(&mut self, reading: [i16; 3]) {
        if !self.debounce.settled(reading) {
            return;
        }
        // Hysteresis: tilting needs the larger angle, levelling out the smaller one
        let threshold = if self.tilted {
            TILT_RELEASE_SIN2
        } else {
            TILT_THRESHOLD_SIN2
        };
        if let Some(tilted) = is_tilted(reading, threshold) {
            self.set_tilted(tilted);
        }
    }

    fn set_tilted(&mut self, tilted: bool) {
        if tilted == self.tilted {
            return;
        }
        self.tilted = tilted;
        if tilted {
            manager::set_pattern(
                Priority::Tilt,
                Pattern::blink(color::COLD_WHITE, BLINK_PERIOD_MS),
            );
        } else {
            manager::clear_pattern(Priority::Tilt);
        }
    }
}

// Also used from the sleeptimer ISR
static TILT: Mutex<RefCell<Tilt>> = Mutex::new(RefCell::new(Tilt {
    monitoring: false,
    enabled: true,
    tilted: false,
    ticks: 0,
    debounce: Debounce::NEW,
}));
static TIMER: SyncCell<sl_sleeptimer_timer_handle_t> = SyncCell::new(ZERO_TIMER);

extern "C" fn monitor_callback(_handle: *mut sl_sleeptimer_timer_handle_t, _data: *mut c_void) {
    let sample_due = critical_section::with(|cs| TILT.borrow(cs).borrow_mut().tick());
    if !sample_due {
        return;
    }
    // Outside the critical section, since I2C takes a while
    let reading = qma6100p_driver::read_raw();
    critical_section::with(|cs| TILT.borrow(cs).borrow_mut().sample(reading));
}

pub fn start() {
    let started = critical_section::with(|cs| {
        !core::mem::replace(&mut TILT.borrow(cs).borrow_mut().monitoring, true)
    });
    if !started {
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
        if !enabled {
            tilt.set_tilted(false);
        }
    });
}
