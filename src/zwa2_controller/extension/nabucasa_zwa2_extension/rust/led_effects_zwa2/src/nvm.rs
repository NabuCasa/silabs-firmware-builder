//! The host-set LED state and configuration, persisted in the application NVM.
use core::ffi::c_void;
use core::mem::size_of;

use crate::bindings::{ZAF_nvm_app_read, ZAF_nvm_app_write, ZPAL_STATUS_OK};
use crate::manager::Rgb;

const FILE_ID_LED: u32 = 0x4660;
const FILE_ID_CONFIG: u32 = 0x4661;

#[repr(C, packed)]
#[derive(Clone, Copy)]
struct LedStorage {
    valid: u8,
    r: u8,
    g: u8,
    b: u8,
}

#[repr(C, packed)]
#[derive(Clone, Copy)]
struct ConfigStorage {
    // A bit per `ConfigKey`, 4 bytes wide like the C enum it replaces
    flags: u32,
}

#[derive(Clone, Copy)]
#[repr(u8)]
pub enum ConfigKey {
    EnableTiltIndicator = 0,
}

impl ConfigKey {
    pub fn from_u8(key: u8) -> Option<Self> {
        match key {
            0 => Some(Self::EnableTiltIndicator),
            _ => None,
        }
    }

    const fn flag(self) -> u32 {
        1 << self as u32
    }
}

const CONFIG_DEFAULTS: ConfigStorage = ConfigStorage {
    flags: ConfigKey::EnableTiltIndicator.flag(),
};

fn read<T: Copy>(file_id: u32) -> Option<T> {
    let mut value = core::mem::MaybeUninit::<T>::uninit();
    let status =
        unsafe { ZAF_nvm_app_read(file_id, value.as_mut_ptr() as *mut c_void, size_of::<T>()) };
    (status == ZPAL_STATUS_OK).then(|| unsafe { value.assume_init() })
}

fn write<T: Copy>(file_id: u32, value: &T) {
    unsafe { ZAF_nvm_app_write(file_id, value as *const T as *const c_void, size_of::<T>()) };
}

/// The configuration bit `key`, for any key the host asks about
pub fn config_flag(key: u8) -> bool {
    let flags = read(FILE_ID_CONFIG).unwrap_or(CONFIG_DEFAULTS).flags;
    1u32.checked_shl(key as u32)
        .is_some_and(|flag| flags & flag != 0)
}

pub fn config_set(key: ConfigKey, value: bool) {
    let mut cfg = read(FILE_ID_CONFIG).unwrap_or(CONFIG_DEFAULTS);
    let flags = cfg.flags;
    cfg.flags = if value {
        flags | key.flag()
    } else {
        flags & !key.flag()
    };
    write(FILE_ID_CONFIG, &cfg);
}

/// Persist the host-set color, so it is restored after a reboot
pub fn store_led(color: Rgb) {
    write(
        FILE_ID_LED,
        &LedStorage {
            valid: 1,
            r: (color.r >> 8) as u8,
            g: (color.g >> 8) as u8,
            b: (color.b >> 8) as u8,
        },
    );
}

/// The persisted host-set color, unless it is fully off
pub fn restore_led() -> Option<Rgb> {
    let stored: LedStorage = read(FILE_ID_LED)?;
    let (valid, r, g, b) = (stored.valid, stored.r, stored.g, stored.b);
    (valid != 0 && (r > 0 || g > 0 || b > 0)).then(|| Rgb::rgb8(r, g, b))
}
