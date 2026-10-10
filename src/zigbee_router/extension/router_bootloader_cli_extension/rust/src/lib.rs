//! Router CLI commands: bootloader info, and rebooting into the bootloader.
#![no_std]

use core::ffi::c_uint;

#[allow(
    non_camel_case_types,
    non_upper_case_globals,
    non_snake_case,
    dead_code
)]
mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::*;

// sl_zigbee_af_app_print, which is a macro
macro_rules! app_print {
    ($format:literal $(, $arg:expr)* $(,)?) => {
        if SL_ZIGBEE_DEBUG_ZCL_GROUP_ENABLED != 0 {
            unsafe { sl_zigbee_af_print(0, $format.as_ptr() $(, $arg)*) }
        }
    };
}

#[no_mangle]
pub extern "C" fn router_bootloader_cli_info(_arguments: *mut sl_cli_command_arg_t) {
    let mut info: BootloaderInformation_t = unsafe { core::mem::zeroed() };
    unsafe { bootloader_getInfo(&mut info) };

    let kind = if info.type_ == SL_BOOTLOADER {
        c"Gecko"
    } else {
        c"Legacy"
    };
    // The version is major.minor.patch in 8, 8 and 16 bits
    app_print!(
        c"%s Bootloader v%u.%02u.%02u\n",
        kind.as_ptr(),
        (info.version >> 24) as c_uint,
        (info.version >> 16 & 0xFF) as c_uint,
        (info.version & 0xFFFF) as c_uint,
    );
}

#[no_mangle]
pub extern "C" fn router_bootloader_cli_reboot(_arguments: *mut sl_cli_command_arg_t) {
    unsafe { bootloader_rebootAndInstall() };
}
