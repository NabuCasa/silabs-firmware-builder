//! The Nabu Casa proprietary Serial API command of the ZWA-2: LED control, system
//! indication, configuration and bootloader info.
#![no_std]

use core::sync::atomic::{AtomicBool, Ordering};

use led_effects_zwa2::{
    clear_pattern, color, config_flag, config_set, set_color, set_pattern, set_tilt_enabled,
    store_led, ConfigKey, Pattern, Priority, Rgb,
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

impl Cmd {
    const ALL: [Cmd; 9] = [
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
}

impl TryFrom<u8> for Cmd {
    type Error = ();

    fn try_from(value: u8) -> Result<Self, ()> {
        Self::ALL
            .into_iter()
            .find(|cmd| *cmd as u8 == value)
            .ok_or(())
    }
}

/// A system indication, shown on the LED's system layer
#[derive(Clone, Copy)]
enum Severity {
    Off,
    Warn,
    Error,
}

impl TryFrom<u8> for Severity {
    type Error = ();

    fn try_from(value: u8) -> Result<Self, ()> {
        match value {
            0 => Ok(Self::Off),
            1 => Ok(Self::Warn),
            2 => Ok(Self::Error),
            _ => Err(()),
        }
    }
}

impl Severity {
    fn pattern(self) -> Option<Pattern> {
        match self {
            Self::Off => None,
            Self::Warn => Some(Pattern::solid(color::YELLOW)),
            Self::Error => Some(Pattern::solid(color::RED)),
        }
    }
}

// Whether the host last set the LED on, for the GET commands
static MANUAL_LED_ON: AtomicBool = AtomicBool::new(false);

/// The LED is cold white or off, whatever color the host asks for
fn manual_color(on: bool) -> Rgb {
    if on {
        color::COLD_WHITE
    } else {
        color::BLACK
    }
}

fn set_manual_led(on: bool) {
    let color = manual_color(on);
    set_color(Priority::Manual, color);
    MANUAL_LED_ON.store(on, Ordering::SeqCst);
    store_led(color);
}

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

    fn extend(&mut self, bytes: &[u8]) {
        self.buf[self.len..self.len + bytes.len()].copy_from_slice(bytes);
        self.len += bytes.len();
    }

    fn bytes(&self) -> &[u8] {
        &self.buf[..self.len]
    }
}

/// Up to four little-endian bytes as a number
fn le_value(bytes: &[u8]) -> u32 {
    let mut padded = [0u8; 4];
    padded[..bytes.len()].copy_from_slice(bytes);
    u32::from_le_bytes(padded)
}

// `args` is the payload after the subcommand byte
fn handle(cmd: Cmd, args: &[u8], response: &mut Response) {
    match cmd {
        Cmd::Supported => {
            // ZW->HOST: a bitmask of the supported subcommands, as few bytes as needed
            let mut bitmask = [0u8; Cmd::BootloaderInfo as usize / 8 + 1];
            for cmd in Cmd::ALL {
                bitmask[cmd as usize / 8] |= 1 << (cmd as usize % 8);
            }
            response.extend(&bitmask);
        }

        Cmd::LedGet => {
            // ZW->HOST: r | g | b
            let color = manual_color(MANUAL_LED_ON.load(Ordering::SeqCst));
            response.extend(&[
                (color.r >> 8) as u8,
                (color.g >> 8) as u8,
                (color.b >> 8) as u8,
            ]);
        }

        Cmd::LedSet => {
            // HOST->ZW: r | g | b
            // ZW->HOST: success
            let ok = match args {
                [r, g, b, ..] => {
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
            let ok = match args {
                [state, ..] => {
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
            let severity = args.first().and_then(|s| Severity::try_from(*s).ok());
            if let Some(severity) = severity {
                match severity.pattern() {
                    Some(pattern) => set_pattern(Priority::System, pattern),
                    None => clear_pattern(Priority::System),
                }
            }
            response.push(severity.is_some() as u8);
        }

        Cmd::ConfigGet => {
            // HOST->ZW: key
            // ZW->HOST: key | size | value, or 0xFF without a key
            match args {
                [key, ..] => response.extend(&[*key, 1, config_flag(*key) as u8]),
                [] => response.push(0xFF),
            }
        }

        Cmd::ConfigSet => {
            // HOST->ZW: key | size | value (little-endian, 1-4 bytes)
            // ZW->HOST: success
            let ok = match args {
                [key, size @ 1..=4, value @ ..] if value.len() >= *size as usize => {
                    let value = le_value(&value[..*size as usize]) != 0;
                    match ConfigKey::try_from(*key) {
                        Ok(key @ ConfigKey::EnableTiltIndicator) => {
                            config_set(key, value);
                            set_tilt_enabled(value);
                            true
                        }
                        Err(()) => false,
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
            response.extend(&[
                (info.version >> BOOTLOADER_VERSION_MAJOR_SHIFT) as u8,
                (info.version >> BOOTLOADER_VERSION_MINOR_SHIFT) as u8,
                info.version as u8,
            ]);
            response.extend(&info.capabilities.to_be_bytes());
        }
    }
}

extern "C" fn handle_nabu_casa(frame: comm_interface_frame_ptr) {
    let frame = unsafe { &*frame };
    // `len` counts the type, command and checksum bytes too
    let payload = &frame.payload[..usize::from(frame.len.saturating_sub(3))];

    let mut response = Response::new();
    match payload.split_first() {
        // No subcommand: `false`
        None => response.push(0),
        Some((&cmd, args)) => {
            response.push(cmd);
            match Cmd::try_from(cmd) {
                Ok(cmd) => handle(cmd, args, &mut response),
                // Unknown subcommand: `false`
                Err(()) => response.push(0),
            }
        }
    }
    let reply = response.bytes();
    unsafe { Respond(frame.cmd, reply.as_ptr(), reply.len() as u8) };
}

// ZW_ADD_CMD: the Serial API collects its handlers from this section
#[used]
#[link_section = "zw_cmd_handlers"]
static CMD_HANDLER: cmd_handler_map_t = cmd_handler_map_t {
    cmd: FUNC_ID_NABU_CASA,
    pHandler: Some(handle_nabu_casa),
};
