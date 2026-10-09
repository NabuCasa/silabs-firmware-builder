//! The host-set LED state and configuration, persisted in the application NVM.
use core::ffi::c_void;
use core::mem::{size_of, MaybeUninit};

use crate::bindings::{ZAF_nvm_app_read, ZAF_nvm_app_write, ZPAL_STATUS_OK};
use crate::manager::Rgb;

/// An object stored under its own file id. Every bit pattern must be a valid value, as it
/// is read straight from the NVM.
trait NvmObject: Copy {
    const FILE_ID: u32;
}

fn read<T: NvmObject>() -> Option<T> {
    let mut value = MaybeUninit::<T>::uninit();
    let status = unsafe {
        ZAF_nvm_app_read(
            T::FILE_ID,
            value.as_mut_ptr() as *mut c_void,
            size_of::<T>(),
        )
    };
    (status == ZPAL_STATUS_OK).then(|| unsafe { value.assume_init() })
}

fn write<T: NvmObject>(value: &T) {
    unsafe {
        ZAF_nvm_app_write(
            T::FILE_ID,
            value as *const T as *const c_void,
            size_of::<T>(),
        )
    };
}

#[repr(C, packed)]
#[derive(Clone, Copy)]
struct LedStorage {
    valid: u8,
    r: u8,
    g: u8,
    b: u8,
}

impl NvmObject for LedStorage {
    const FILE_ID: u32 = 0x4660;
}

impl From<Rgb> for LedStorage {
    fn from(color: Rgb) -> Self {
        Self {
            valid: 1,
            r: (color.r >> 8) as u8,
            g: (color.g >> 8) as u8,
            b: (color.b >> 8) as u8,
        }
    }
}

#[derive(Clone, Copy)]
#[repr(u8)]
pub enum ConfigKey {
    EnableTiltIndicator = 0,
}

impl TryFrom<u8> for ConfigKey {
    type Error = ();

    fn try_from(key: u8) -> Result<Self, ()> {
        match key {
            0 => Ok(Self::EnableTiltIndicator),
            _ => Err(()),
        }
    }
}

impl ConfigKey {
    const fn flag(self) -> u32 {
        1 << self as u32
    }
}

#[repr(C, packed)]
#[derive(Clone, Copy)]
struct ConfigStorage {
    // A bit per `ConfigKey`, 4 bytes wide like the C enum it replaces
    flags: u32,
}

impl NvmObject for ConfigStorage {
    const FILE_ID: u32 = 0x4661;
}

impl Default for ConfigStorage {
    fn default() -> Self {
        Self {
            flags: ConfigKey::EnableTiltIndicator.flag(),
        }
    }
}

impl ConfigStorage {
    fn load() -> Self {
        read().unwrap_or_default()
    }
}

/// The configuration bit `key`, for any key the host asks about
pub fn config_flag(key: u8) -> bool {
    let flags = ConfigStorage::load().flags;
    1u32.checked_shl(key.into())
        .is_some_and(|flag| flags & flag != 0)
}

pub fn config_set(key: ConfigKey, value: bool) {
    let mut cfg = ConfigStorage::load();
    let flags = cfg.flags;
    cfg.flags = if value {
        flags | key.flag()
    } else {
        flags & !key.flag()
    };
    write(&cfg);
}

/// Persist the host-set color, so it is restored after a reboot
pub fn store_led(color: Rgb) {
    write(&LedStorage::from(color));
}

/// The persisted host-set color, unless it is fully off
pub fn restore_led() -> Option<Rgb> {
    let LedStorage { valid, r, g, b } = read()?;
    (valid != 0 && (r > 0 || g > 0 || b > 0)).then(|| Rgb::rgb8(r, g, b))
}
