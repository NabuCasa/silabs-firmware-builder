//! ZBT-2 XNCP commands: LED control (0x0F00) and accelerometer read (0x0F01).
#![no_std]

use led_effects_base::{set_color, Priority, Rgb};
use qma6100p_driver::read_acceleration;
use xncp_core::{ReplyBuf, Status, XncpFeature, XncpResult};
use xncp_core::xncp_command;

#[xncp_command(0x0F00, feature = XncpFeature::LedControl)]
fn handle_set_led_state(req: &[u8], _reply: &mut ReplyBuf) -> XncpResult {
    let color = match req {
        // One byte per channel, taken as the high byte.
        &[r, g, b] => Rgb {
            r: (r as u16) << 8,
            g: (g as u16) << 8,
            b: (b as u16) << 8,
        },
        // Full 16-bit per channel, big-endian.
        &[rh, rl, gh, gl, bh, bl] => Rgb {
            r: u16::from_be_bytes([rh, rl]),
            g: u16::from_be_bytes([gh, gl]),
            b: u16::from_be_bytes([bh, bl]),
        },
        _ => return Err(Status::BAD_ARGUMENT),
    };

    set_color(Priority::Manual, color);
    Ok(())
}

#[xncp_command(0x0F01, feature = XncpFeature::LedControl, allow_duplicate = true)]
fn handle_get_accelerometer(_req: &[u8], reply: &mut ReplyBuf) -> XncpResult {
    for value in read_acceleration() {
        reply.push_bytes(&value.to_le_bytes());
    }
    Ok(())
}
