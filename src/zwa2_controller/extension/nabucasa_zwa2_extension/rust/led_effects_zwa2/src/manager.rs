//! The priority-layered LED manager, driven from a sleeptimer.
use core::cell::RefCell;
use core::ffi::c_void;
use core::ops::{Index, IndexMut};
use core::sync::atomic::{AtomicBool, AtomicU32, Ordering};

use critical_section::Mutex;
use ohf_sys::{
    sl_led_rgb_pwm_t, sl_led_set_rgb_color, sl_led_t, sl_led_turn_off, sl_led_turn_on,
    sl_sleeptimer_start_periodic_timer_ms, sl_sleeptimer_timer_handle_t, SyncCell, ZERO_TIMER,
};

pub const UPDATE_INTERVAL_MS: u32 = 4;

#[derive(Clone, Copy)]
pub struct Rgb {
    pub r: u16,
    pub g: u16,
    pub b: u16,
}

impl Rgb {
    pub const fn rgb8(r: u8, g: u8, b: u8) -> Self {
        Self {
            r: r as u16 * 257,
            g: g as u16 * 257,
            b: b as u16 * 257,
        }
    }

    // `brightness` is 0-65535
    fn scaled(self, brightness: u32) -> Self {
        let scale = |c: u16| ((c as u32 * brightness) / 65535) as u16;
        Self {
            r: scale(self.r),
            g: scale(self.g),
            b: scale(self.b),
        }
    }
}

pub mod color {
    use super::Rgb;

    pub const COLD_WHITE: Rgb = Rgb::rgb8(0xFF, 0xFF, 0xFF);
    /// Not fully off: a slight green glow shows the board is powered
    pub const BLACK: Rgb = Rgb::rgb8(0x00, 0x02, 0x00);
    pub const RED: Rgb = Rgb::rgb8(0xFF, 0x00, 0x00);
    pub const YELLOW: Rgb = Rgb::rgb8(0xFF, 0x80, 0x00);
}

/// Higher layers hide lower ones while active
#[derive(Clone, Copy)]
pub enum Priority {
    /// Host connection state
    Background,
    /// Host LED commands
    Manual,
    Tilt,
    /// Host system indications: warning, error
    System,
}

impl Priority {
    const COUNT: usize = Priority::System as usize + 1;
}

#[derive(Clone, Copy)]
enum Mode {
    Static,
    /// On/off square wave
    Blink {
        period_ms: u16,
    },
    /// Triangle wave fading between two brightnesses (0-65535)
    Pulse {
        period_ms: u16,
        brightness_min: u16,
        brightness_max: u16,
    },
}

#[derive(Clone, Copy)]
pub struct Pattern {
    mode: Mode,
    color: Rgb,
}

impl Pattern {
    pub const fn solid(color: Rgb) -> Self {
        Self {
            mode: Mode::Static,
            color,
        }
    }

    pub const fn blink(color: Rgb, period_ms: u16) -> Self {
        assert!(period_ms >= 2);
        Self {
            mode: Mode::Blink { period_ms },
            color,
        }
    }

    pub const fn pulse(
        color: Rgb,
        period_ms: u16,
        brightness_min: u16,
        brightness_max: u16,
    ) -> Self {
        assert!(period_ms >= 2 && brightness_min <= brightness_max);
        Self {
            mode: Mode::Pulse {
                period_ms,
                brightness_min,
                brightness_max,
            },
            color,
        }
    }

    /// The color `elapsed_ms` into the pattern, `None` for off
    fn color_at(&self, elapsed_ms: u32) -> Option<Rgb> {
        match self.mode {
            Mode::Static => Some(self.color),
            Mode::Blink { period_ms } => {
                let period = period_ms as u32;
                (elapsed_ms % period < period / 2).then_some(self.color)
            }
            Mode::Pulse {
                period_ms,
                brightness_min,
                brightness_max,
            } => {
                let period = period_ms as u32;
                let half = period / 2;
                let phase = elapsed_ms % period;
                let triangle = if phase < half {
                    phase * 65535 / half
                } else {
                    65535 - (phase - half) * 65535 / (period - half)
                };
                let (min, max) = (brightness_min as u32, brightness_max as u32);
                Some(self.color.scaled(min + (max - min) * triangle / 65535))
            }
        }
    }
}

extern "C" {
    static sl_led_ws2812: sl_led_rgb_pwm_t;
    fn ws2812_led_driver_refresh();
}

#[derive(Clone, Copy)]
struct Layer {
    pattern: Pattern,
    start_tick: u32,
}

struct Layers([Option<Layer>; Priority::COUNT]);

impl Layers {
    /// The highest active layer
    fn top(&self) -> Option<Layer> {
        self.0.iter().rev().flatten().next().copied()
    }
}

impl Index<Priority> for Layers {
    type Output = Option<Layer>;

    fn index(&self, priority: Priority) -> &Option<Layer> {
        &self.0[priority as usize]
    }
}

impl IndexMut<Priority> for Layers {
    fn index_mut(&mut self, priority: Priority) -> &mut Option<Layer> {
        &mut self.0[priority as usize]
    }
}

// Also used from the sleeptimer ISR
static LAYERS: Mutex<RefCell<Layers>> = Mutex::new(RefCell::new(Layers([None; Priority::COUNT])));
static TICK: AtomicU32 = AtomicU32::new(0);
static INITIALIZED: AtomicBool = AtomicBool::new(false);
static TIMER: SyncCell<sl_sleeptimer_timer_handle_t> = SyncCell::new(ZERO_TIMER);

fn led_common() -> *const sl_led_t {
    unsafe { &sl_led_ws2812.led_common as *const sl_led_t }
}

fn show(color: Option<Rgb>) {
    match color {
        Some(color) => unsafe {
            sl_led_set_rgb_color(&sl_led_ws2812, color.r, color.g, color.b);
            sl_led_turn_on(led_common());
        },
        None => unsafe { sl_led_turn_off(led_common()) },
    }
}

fn update_led_hardware() {
    let color = critical_section::with(|cs| {
        // Read in the critical section, so a layer can't start after `tick`
        let tick = TICK.load(Ordering::SeqCst);
        let layer = LAYERS.borrow(cs).borrow().top()?;
        let elapsed_ms = tick
            .wrapping_sub(layer.start_tick)
            .wrapping_mul(UPDATE_INTERVAL_MS);
        layer.pattern.color_at(elapsed_ms)
    });
    show(color);
}

extern "C" fn timer_callback(_handle: *mut sl_sleeptimer_timer_handle_t, _data: *mut c_void) {
    TICK.fetch_add(1, Ordering::SeqCst);
    update_led_hardware();
    unsafe { ws2812_led_driver_refresh() }
}

pub fn init() {
    if INITIALIZED.swap(true, Ordering::SeqCst) {
        return;
    }
    unsafe {
        sl_sleeptimer_start_periodic_timer_ms(
            TIMER.get(),
            UPDATE_INTERVAL_MS,
            Some(timer_callback),
            core::ptr::null_mut(),
            0,
            0,
        );
    }
}

pub fn set_pattern(priority: Priority, pattern: Pattern) {
    critical_section::with(|cs| {
        let tick = TICK.load(Ordering::SeqCst);
        LAYERS.borrow(cs).borrow_mut()[priority] = Some(Layer {
            pattern,
            start_tick: tick,
        });
    });
}

pub fn clear_pattern(priority: Priority) {
    critical_section::with(|cs| LAYERS.borrow(cs).borrow_mut()[priority] = None);
}

pub fn set_color(priority: Priority, color: Rgb) {
    set_pattern(priority, Pattern::solid(color));
}
