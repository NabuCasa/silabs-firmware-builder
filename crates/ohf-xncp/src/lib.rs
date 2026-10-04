//! XNCP core — Rust port of `xncp_core.c` + the Jinja dispatcher template.
//!
//! The custom-frame callback parses the frame, answers `get_supported_features`, and
//! dispatches to command handlers. Command sets (common, ZBT-2, …) register into
//! `XNCP_COMMANDS` / `XNCP_FEATURES` at link time via `linkme::distributed_slice` — no
//! codegen, and only the sets SLC enabled are linked in, so mutually-exclusive sets can't
//! collide.
//!
//! The wire protocol (`{command_id, status}` framing, feature bitmask, the
//! `sl_zigbee_af_xncp_incoming_custom_frame_cb` entry) is preserved exactly.
#![no_std]
#![allow(non_camel_case_types)]

use linkme::distributed_slice;

// --- xncp_types.h mirror: the C ABI shared with the still-C common command set ---
#[repr(C)]
pub struct XncpContext {
    pub command_id: u16,
    pub payload: *mut u8,
    pub payload_length: u8,
    pub reply: *mut u8,
    pub reply_length: *mut u8,
    pub status: *mut u8,
    pub response_id: *mut u16,
}

pub type XncpHandler = unsafe extern "C" fn(ctx: *mut XncpContext) -> bool;

#[repr(C)]
pub struct XncpCommandDef {
    pub command_id: u16,
    pub handler: Option<XncpHandler>,
}

// Feature flags — command sets contribute the ones they implement.
pub const XNCP_FEATURE_MEMBER_OF_ALL_GROUPS: u32 = 1 << 0;
pub const XNCP_FEATURE_MANUAL_SOURCE_ROUTE: u32 = 1 << 1;
pub const XNCP_FEATURE_MFG_TOKEN_OVERRIDES: u32 = 1 << 2;
pub const XNCP_FEATURE_BUILD_STRING: u32 = 1 << 3;
pub const XNCP_FEATURE_FLOW_CONTROL_TYPE: u32 = 1 << 4;
pub const XNCP_FEATURE_CHIP_INFO: u32 = 1 << 5;
pub const XNCP_FEATURE_RESTORE_ROUTE_TABLE: u32 = 1 << 6;
pub const XNCP_FEATURE_TX_POWER_INFO: u32 = 1 << 7;
pub const XNCP_FEATURE_COMBINED_SEND: u32 = 1 << 8;
pub const XNCP_FEATURE_LED_CONTROL: u32 = 1 << 31;

const XNCP_CMD_GET_SUPPORTED_FEATURES_REQ: u16 = 0x0000;
const XNCP_CMD_UNKNOWN: u16 = 0xFFFF;
const XNCP_CMD_RESPONSE_BIT: u16 = 0x8000;

// Low byte of the sl_status_t the wire protocol carries (xncp_core.c truncated to u8).
const XNCP_STATUS_OK: u8 = 0x00; // SL_STATUS_OK
const XNCP_STATUS_BAD_ARGUMENT: u8 = 0x21; // SL_STATUS_INVALID_PARAMETER
const XNCP_STATUS_NOT_FOUND: u8 = 0x25; // SL_STATUS_NOT_FOUND

/// Command handlers, collected across all enabled command-set crates at link time.
#[distributed_slice]
pub static XNCP_COMMANDS: [XncpCommandDef];

/// Feature bits, OR-ed into the reported mask.
#[distributed_slice]
pub static XNCP_FEATURES: [u32];

fn supported_features() -> u32 {
    let mut features = 0;
    for &bit in XNCP_FEATURES.iter() {
        features |= bit;
    }
    features
}

unsafe fn dispatch(ctx: *mut XncpContext) -> bool {
    let command_id = (*ctx).command_id;

    for cmd in XNCP_COMMANDS.iter() {
        if cmd.command_id == command_id {
            if let Some(handler) = cmd.handler {
                *(*ctx).response_id = command_id | XNCP_CMD_RESPONSE_BIT;
                return handler(ctx);
            }
        }
    }

    false
}

// The SDK declares sl_zigbee_af_xncp_incoming_custom_frame_cb as SL_WEAK. Rather than a C
// shim owning that name, we linker-wrap it: `-Wl,--wrap=sl_zigbee_af_xncp_incoming_custom_frame_cb`
// redirects the stack's calls to `__wrap_…` (this fn), and the `--wrap` reference pulls it
// out of the aggregate archive on its own. lld honours the wrap even though the weak default
// is visible to LTO; GNU ld would call the visible definition directly (binutils PR ld/31956),
// so this path is LLVM-only.
#[no_mangle]
pub unsafe extern "C" fn __wrap_sl_zigbee_af_xncp_incoming_custom_frame_cb(
    message_length: u8,
    message_payload: *mut u8,
    reply_payload_length: *mut u8,
    reply_payload: *mut u8,
) -> u32 {
    let mut rsp_status: u8 = XNCP_STATUS_OK;
    let mut rsp_command_id: u16 = XNCP_CMD_UNKNOWN;

    'respond: {
        if message_length < 3 {
            rsp_status = XNCP_STATUS_BAD_ARGUMENT;
            break 'respond;
        }

        let req_command_id =
            (*message_payload as u16) | ((*message_payload.add(1) as u16) << 8);
        // messagePayload[2] (req status) is unused. Strip the 3-byte header.
        let payload = message_payload.add(3);
        let payload_length = message_length - 3;

        // Leave space for the reply header.
        *reply_payload_length = 3;

        if req_command_id == XNCP_CMD_GET_SUPPORTED_FEATURES_REQ {
            let features = supported_features();
            rsp_command_id = XNCP_CMD_GET_SUPPORTED_FEATURES_REQ | XNCP_CMD_RESPONSE_BIT;
            *reply_payload.add(3) = (features & 0xFF) as u8;
            *reply_payload.add(4) = ((features >> 8) & 0xFF) as u8;
            *reply_payload.add(5) = ((features >> 16) & 0xFF) as u8;
            *reply_payload.add(6) = ((features >> 24) & 0xFF) as u8;
            *reply_payload_length = 7;
            break 'respond;
        }

        let mut ctx = XncpContext {
            command_id: req_command_id,
            payload,
            payload_length,
            reply: reply_payload,
            reply_length: reply_payload_length,
            status: &mut rsp_status,
            response_id: &mut rsp_command_id,
        };

        if !dispatch(&mut ctx) {
            rsp_status = XNCP_STATUS_NOT_FOUND;
        }
    }

    *reply_payload.add(0) = (rsp_command_id & 0xFF) as u8;
    *reply_payload.add(1) = ((rsp_command_id >> 8) & 0xFF) as u8;
    *reply_payload.add(2) = rsp_status;
    XNCP_STATUS_OK as u32
}
