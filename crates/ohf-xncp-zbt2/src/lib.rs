//! ZBT-2 XNCP commands — Rust port of `xncp_zbt2_commands.c`.
//!
//! LED control (0x0F00) and accelerometer read (0x0F01). Registers into the XNCP core's
//! linkme slices; calls the already-Rust LED/accel crates directly.
#![no_std]

use ohf_led_effects::{led_manager_set_color, rgb_t};
use ohf_qma6100p::qma6100p_read_acc_xyz;
use ohf_sys::I2C_TypeDef;
use ohf_xncp::{
    XncpCommandDef, XncpContext, XNCP_COMMANDS, XNCP_FEATURES, XNCP_FEATURE_LED_CONTROL,
    XNCP_FEATURE_TX_POWER_INFO,
};

use linkme::distributed_slice;

const LED_PRIORITY_MANUAL: u32 = 1;
const XNCP_STATUS_OK: u8 = 0x00;
const XNCP_STATUS_BAD_ARGUMENT: u8 = 0x21;

extern "C" {
    static sl_i2cspm_inst: *mut I2C_TypeDef;
}

unsafe extern "C" fn handle_set_led_state(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;
    let p = ctx.payload;

    let color = match ctx.payload_length {
        // 3 bytes: one byte per channel, taken as the high byte.
        3 => rgb_t {
            r: (*p.add(0) as u16) << 8,
            g: (*p.add(1) as u16) << 8,
            b: (*p.add(2) as u16) << 8,
        },
        // 6 bytes: full 16-bit per channel, big-endian.
        6 => rgb_t {
            r: ((*p.add(0) as u16) << 8) | *p.add(1) as u16,
            g: ((*p.add(2) as u16) << 8) | *p.add(3) as u16,
            b: ((*p.add(4) as u16) << 8) | *p.add(5) as u16,
        },
        _ => {
            *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
            return true;
        }
    };

    led_manager_set_color(LED_PRIORITY_MANUAL, color);
    *ctx.status = XNCP_STATUS_OK;
    true
}

unsafe extern "C" fn handle_get_accelerometer(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;
    let mut xyz = [0.0f32; 3];
    qma6100p_read_acc_xyz(sl_i2cspm_inst, xyz.as_mut_ptr());

    for value in xyz {
        let off = *ctx.reply_length as usize;
        core::ptr::copy_nonoverlapping(value.to_ne_bytes().as_ptr(), ctx.reply.add(off), 4);
        *ctx.reply_length += 4;
    }

    *ctx.status = XNCP_STATUS_OK;
    true
}

#[distributed_slice(XNCP_COMMANDS)]
static SET_LED_STATE: XncpCommandDef =
    XncpCommandDef { command_id: 0x0F00, handler: Some(handle_set_led_state) };

#[distributed_slice(XNCP_COMMANDS)]
static GET_ACCELEROMETER: XncpCommandDef =
    XncpCommandDef { command_id: 0x0F01, handler: Some(handle_get_accelerometer) };

#[distributed_slice(XNCP_FEATURES)]
static FEATURE_LED_CONTROL: u32 = XNCP_FEATURE_LED_CONTROL;

#[distributed_slice(XNCP_FEATURES)]
static FEATURE_TX_POWER_INFO: u32 = XNCP_FEATURE_TX_POWER_INFO;
