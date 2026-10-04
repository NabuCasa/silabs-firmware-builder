//! ZBT-2 XNCP commands — LED control (0x0F00) and accelerometer read (0x0F01).
#![no_std]

use linkme::distributed_slice;

use ohf_led_effects::{led_manager_set_color, rgb_t};
use ohf_qma6100p::read_acceleration;
use ohf_xncp::{
    ReplyBuf, Status, XncpCommandDef, XNCP_COMMANDS, XNCP_FEATURES, XNCP_FEATURE_LED_CONTROL,
    XNCP_FEATURE_TX_POWER_INFO,
};

const LED_PRIORITY_MANUAL: u32 = 1;

fn handle_set_led_state(req: &[u8], _reply: &mut ReplyBuf) -> Status {
    let color = match req {
        // One byte per channel, taken as the high byte.
        &[r, g, b] => rgb_t {
            r: (r as u16) << 8,
            g: (g as u16) << 8,
            b: (b as u16) << 8,
        },
        // Full 16-bit per channel, big-endian.
        &[rh, rl, gh, gl, bh, bl] => rgb_t {
            r: u16::from_be_bytes([rh, rl]),
            g: u16::from_be_bytes([gh, gl]),
            b: u16::from_be_bytes([bh, bl]),
        },
        _ => return Status::BAD_ARGUMENT,
    };

    led_manager_set_color(LED_PRIORITY_MANUAL, color);
    Status::OK
}

fn handle_get_accelerometer(_req: &[u8], reply: &mut ReplyBuf) -> Status {
    for value in read_acceleration() {
        reply.push_bytes(&value.to_le_bytes());
    }
    Status::OK
}

#[distributed_slice(XNCP_COMMANDS)]
static SET_LED_STATE: XncpCommandDef = XncpCommandDef {
    command_id: 0x0F00,
    handler: handle_set_led_state,
};

#[distributed_slice(XNCP_COMMANDS)]
static GET_ACCELEROMETER: XncpCommandDef = XncpCommandDef {
    command_id: 0x0F01,
    handler: handle_get_accelerometer,
};

#[distributed_slice(XNCP_FEATURES)]
static FEATURE_LED_CONTROL: u32 = XNCP_FEATURE_LED_CONTROL;

#[distributed_slice(XNCP_FEATURES)]
static FEATURE_TX_POWER_INFO: u32 = XNCP_FEATURE_TX_POWER_INFO;
