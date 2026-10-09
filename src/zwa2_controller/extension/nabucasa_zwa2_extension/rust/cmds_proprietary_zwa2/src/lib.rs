//! The Nabu Casa proprietary Serial API command of the ZWA-2: LED control, system
//! indication, configuration and bootloader info.
#![no_std]

use core::sync::atomic::{AtomicBool, Ordering};

use led_effects_zwa2::{
    clear_pattern, color, config_flag, config_set, set_color, set_pattern, set_tilt_enabled,
    store_led, ConfigKey, Pattern, Priority,
};

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

const FUNC_ID_NABU_CASA: u8 = FUNC_ID_PROPRIETARY_0 as u8;

/// The subcommand, in the first payload byte. The response echoes it.
#[derive(Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
enum Cmd {
    Supported = 0,
    LedGet = 1,
    LedSet = 2,
    // GyroMeasure (3) is obsolete
    SystemIndicationSet = 4,
    ConfigGet = 5,
    ConfigSet = 6,
    LedGetBinary = 7,
    LedSetBinary = 8,
    BootloaderInfo = 9,
}

const SUPPORTED: [Cmd; 9] = [
    Cmd::Supported,
    Cmd::LedGet,
    Cmd::LedSet,
    Cmd::SystemIndicationSet,
    Cmd::ConfigGet,
    Cmd::ConfigSet,
    Cmd::LedGetBinary,
    Cmd::LedSetBinary,
    Cmd::BootloaderInfo,
];

impl Cmd {
    fn from_u8(value: u8) -> Option<Self> {
        SUPPORTED.into_iter().find(|cmd| *cmd as u8 == value)
    }
}

const SYS_INDICATION_OFF: u8 = 0;
const SYS_INDICATION_WARN: u8 = 1;
const SYS_INDICATION_ERROR: u8 = 2;

// Whether the host last set the LED on, for the GET commands
static MANUAL_LED_ON: AtomicBool = AtomicBool::new(false);

struct Response {
    buf: [u8; BUF_SIZE_TX],
    len: usize,
}

impl Response {
    const fn new() -> Self {
        Self {
            buf: [0; BUF_SIZE_TX],
            len: 0,
        }
    }

    fn push(&mut self, byte: u8) {
        self.buf[self.len] = byte;
        self.len += 1;
    }

    fn push_bytes(&mut self, bytes: &[u8]) {
        for byte in bytes {
            self.push(*byte);
        }
    }
}

/// The LED is cold white or off, whatever color the host asks for
fn set_manual_led(on: bool) {
    let color = if on { color::COLD_WHITE } else { color::BLACK };
    set_color(Priority::Manual, color);
    MANUAL_LED_ON.store(on, Ordering::SeqCst);
    store_led(color);
}

fn manual_led_color() -> led_effects_zwa2::Rgb {
    if MANUAL_LED_ON.load(Ordering::SeqCst) {
        color::COLD_WHITE
    } else {
        color::BLACK
    }
}

// `req` starts with the subcommand byte
fn handle(cmd: Cmd, req: &[u8], response: &mut Response) {
    match cmd {
        Cmd::Supported => {
            // ZW->HOST: a bitmask of the supported subcommands, as few bytes as needed
            let mut bitmask = [0u8; Cmd::BootloaderInfo as usize / 8 + 1];
            for cmd in SUPPORTED {
                bitmask[cmd as usize / 8] |= 1 << (cmd as usize % 8);
            }
            response.push_bytes(&bitmask);
        }

        Cmd::LedGet => {
            // ZW->HOST: r | g | b
            let color = manual_led_color();
            response.push_bytes(&[
                (color.r >> 8) as u8,
                (color.g >> 8) as u8,
                (color.b >> 8) as u8,
            ]);
        }

        Cmd::LedSet => {
            // HOST->ZW: r | g | b
            // ZW->HOST: success
            let ok = match req {
                [_, r, g, b, ..] => {
                    set_manual_led(*r > 0 || *g > 0 || *b > 0);
                    true
                }
                _ => false,
            };
            response.push(ok as u8);
        }

        Cmd::LedGetBinary => {
            // ZW->HOST: state
            response.push(MANUAL_LED_ON.load(Ordering::SeqCst) as u8);
        }

        Cmd::LedSetBinary => {
            // HOST->ZW: state
            // ZW->HOST: success
            let ok = match req {
                [_, state, ..] => {
                    set_manual_led(*state != 0);
                    true
                }
                _ => false,
            };
            response.push(ok as u8);
        }

        Cmd::SystemIndicationSet => {
            // HOST->ZW: severity
            // ZW->HOST: success
            let ok = match req {
                [_, SYS_INDICATION_OFF, ..] => {
                    clear_pattern(Priority::System);
                    true
                }
                [_, SYS_INDICATION_WARN, ..] => {
                    set_pattern(Priority::System, Pattern::solid(color::YELLOW));
                    true
                }
                [_, SYS_INDICATION_ERROR, ..] => {
                    set_pattern(Priority::System, Pattern::solid(color::RED));
                    true
                }
                _ => false,
            };
            response.push(ok as u8);
        }

        Cmd::ConfigGet => {
            // HOST->ZW: key
            // ZW->HOST: key | size | value, or 0xFF without a key
            match req {
                [_, key, ..] => response.push_bytes(&[*key, 1, config_flag(*key) as u8]),
                _ => response.push(0xFF),
            }
        }

        Cmd::ConfigSet => {
            // HOST->ZW: key | size | value (little-endian, 1-4 bytes)
            // ZW->HOST: success
            let ok = match req {
                [_, key, size @ 1..=4, value @ ..] if value.len() >= *size as usize => {
                    let value = value[..*size as usize]
                        .iter()
                        .rev()
                        .fold(0u32, |acc, byte| acc << 8 | *byte as u32);
                    match ConfigKey::from_u8(*key) {
                        Some(key @ ConfigKey::EnableTiltIndicator) => {
                            config_set(key, value != 0);
                            set_tilt_enabled(value != 0);
                            true
                        }
                        None => false,
                    }
                }
                _ => false,
            };
            response.push(ok as u8);
        }

        Cmd::BootloaderInfo => {
            // ZW->HOST: major | minor | customer | capabilities (big-endian)
            // bootloader_getInfo() leaves the version untouched when it finds no bootloader
            // table, so the initializer defines what gets reported.
            let mut info = BootloaderInformation_t {
                type_: NO_BOOTLOADER,
                version: 0,
                capabilities: 0,
            };
            unsafe { bootloader_getInfo(&mut info) };
            // The SDK's customer field is 16 bits wide, but only its low byte is sent
            // because the version is consumed as major.minor.customer.
            response.push((info.version >> BOOTLOADER_VERSION_MAJOR_SHIFT) as u8);
            response.push((info.version >> BOOTLOADER_VERSION_MINOR_SHIFT) as u8);
            response.push(info.version as u8);
            response.push_bytes(&info.capabilities.to_be_bytes());
        }
    }
}

extern "C" fn handle_nabu_casa(frame: comm_interface_frame_ptr) {
    let frame = unsafe { &*frame };
    // `len` counts the type, command and checksum bytes too
    let req = &frame.payload[..usize::from(frame.len.saturating_sub(3))];

    let mut response = Response::new();
    match req.first().copied().map(Cmd::from_u8) {
        Some(Some(cmd)) => {
            response.push(cmd as u8);
            handle(cmd, req, &mut response);
        }
        // Unknown subcommand: echo it with `false`
        Some(None) => response.push_bytes(&[req[0], 0]),
        // No subcommand: `false`
        None => response.push(0),
    }
    unsafe { Respond(frame.cmd, response.buf.as_ptr(), response.len as u8) };
}

// ZW_ADD_CMD: the Serial API collects its handlers from this section
#[used]
#[link_section = "zw_cmd_handlers"]
static CMD_HANDLER: cmd_handler_map_t = cmd_handler_map_t {
    cmd: FUNC_ID_NABU_CASA,
    pHandler: Some(handle_nabu_casa),
};
