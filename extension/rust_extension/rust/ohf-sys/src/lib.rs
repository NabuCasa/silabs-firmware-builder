//! SDK bindings shared by the components.
#![no_std]
#![allow(
    non_camel_case_types,
    non_upper_case_globals,
    non_snake_case,
    dead_code
)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

use core::cell::UnsafeCell;
use core::mem::MaybeUninit;

/// A static that the SDK or a single context writes to
pub struct SyncCell<T>(UnsafeCell<T>);
unsafe impl<T> Sync for SyncCell<T> {}

impl<T> SyncCell<T> {
    pub const fn new(v: T) -> Self {
        Self(UnsafeCell::new(v))
    }

    pub fn get(&self) -> *mut T {
        self.0.get()
    }
}

pub const ZERO_TIMER: sl_sleeptimer_timer_handle_t = unsafe { MaybeUninit::zeroed().assume_init() };

extern "C" {
    /// `NVIC_SystemReset`, which is inline. The shim is a source of the `ohf_sys` component.
    pub fn ohf_system_reset() -> !;
}

/// The token macros paste in the size, so call the functions behind them.
#[cfg(feature = "tokens")]
pub mod token {
    use core::ffi::c_void;
    use core::mem::{size_of, MaybeUninit};

    const NO_INDEX: u8 = 0x7F;

    // bindgen emits the TOKEN_* keys as u32; the halInternal* API takes u16.
    fn key(token: u32) -> u16 {
        u16::try_from(token).unwrap()
    }

    pub fn get<T: Copy>(token: u32) -> T {
        let mut v = MaybeUninit::<T>::uninit();
        unsafe {
            super::halInternalGetTokenData(
                v.as_mut_ptr() as *mut c_void,
                key(token),
                NO_INDEX,
                size_of::<T>() as u8,
            );
            v.assume_init()
        }
    }

    /// Whether the stack has network settings stored
    pub fn has_stored_network() -> bool {
        let node: super::tokTypeStackNodeData = get(super::TOKEN_STACK_NODE_DATA);
        node.panId != 0xFFFF && (11..=26).contains(&node.radioFreqChannel)
    }
}

/// Erase NVM3 and the Zigbee PSA keys, then reset.
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
