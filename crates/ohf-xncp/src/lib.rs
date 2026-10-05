//! XNCP custom-frame dispatch. Command sets register into `XNCP_COMMANDS` and
//! `XNCP_FEATURES`.
#![no_std]
#![allow(non_camel_case_types)]

// For `ohf_xncp_macros::xncp_command`
pub use linkme::distributed_slice;

/// The wire status byte: the low byte of the `sl_status_t` the protocol carries.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Status(pub u8);

impl Status {
    pub const OK: Status = Status(0x00); // SL_STATUS_OK
    pub const BAD_ARGUMENT: Status = Status(0x21); // SL_STATUS_INVALID_PARAMETER
    pub const NOT_FOUND: Status = Status(0x25); // SL_STATUS_NOT_FOUND
}

/// Bounds-checked cursor over the request payload
pub struct Reader<'a> {
    buf: &'a [u8],
    pos: usize,
}

impl<'a> Reader<'a> {
    pub fn new(buf: &'a [u8]) -> Self {
        Self { buf, pos: 0 }
    }

    pub fn remaining(&self) -> usize {
        self.buf.len() - self.pos
    }

    pub fn u8(&mut self) -> Option<u8> {
        let v = *self.buf.get(self.pos)?;
        self.pos += 1;
        Some(v)
    }

    pub fn u16_le(&mut self) -> Option<u16> {
        let b = self.buf.get(self.pos..self.pos + 2)?;
        self.pos += 2;
        Some(u16::from_le_bytes([b[0], b[1]]))
    }

    pub fn bytes(&mut self, n: usize) -> Option<&'a [u8]> {
        let b = self.buf.get(self.pos..self.pos + n)?;
        self.pos += n;
        Some(b)
    }

    /// The not-yet-read remainder.
    pub fn rest(&self) -> &'a [u8] {
        &self.buf[self.pos..]
    }
}

/// Writer over the reply buffer. Overflow panics: replies are capped at 119 bytes.
pub struct ReplyBuf<'a> {
    buf: &'a mut [u8],
    len: usize,
}

impl<'a> ReplyBuf<'a> {
    pub fn new(buf: &'a mut [u8]) -> Self {
        Self { buf, len: 0 }
    }

    pub fn push(&mut self, byte: u8) {
        self.buf[self.len] = byte;
        self.len += 1;
    }

    pub fn push_bytes(&mut self, bytes: &[u8]) {
        self.buf[self.len..self.len + bytes.len()].copy_from_slice(bytes);
        self.len += bytes.len();
    }

    pub fn push_u16_le(&mut self, v: u16) {
        self.push_bytes(&v.to_le_bytes());
    }

    pub fn push_u32_le(&mut self, v: u32) {
        self.push_bytes(&v.to_le_bytes());
    }

    pub fn len(&self) -> usize {
        self.len
    }
}

/// A command handler: parse `req`, write the response payload into `reply`, return the status.
pub type XncpHandler = fn(req: &[u8], reply: &mut ReplyBuf) -> Status;

pub struct XncpCommandDef {
    pub command_id: u16,
    pub handler: XncpHandler,
}

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

// Custom-frame replies are capped at 119 bytes
const REPLY_BUF_LEN: usize = 119;
// {response_id: u16 le, status: u8}
const REPLY_HEADER_LEN: usize = 3;

/// Command handlers, collected across all enabled command-set crates at link time.
#[distributed_slice]
pub static XNCP_COMMANDS: [XncpCommandDef];

/// Feature bits, OR-ed into the reported mask.
#[distributed_slice]
pub static XNCP_FEATURES: [u32];

fn supported_features() -> u32 {
    XNCP_FEATURES.iter().fold(0, |acc, &bit| acc | bit)
}

/// Returns the `(response_id, status)` for the reply header.
fn dispatch(message: &[u8], reply: &mut ReplyBuf) -> (u16, Status) {
    // frame = {command_id: u16 le, req_status: u8 (unused), payload…}
    if message.len() < 3 {
        return (XNCP_CMD_UNKNOWN, Status::BAD_ARGUMENT);
    }
    let command_id = u16::from_le_bytes([message[0], message[1]]);
    let payload = &message[3..];

    if command_id == XNCP_CMD_GET_SUPPORTED_FEATURES_REQ {
        reply.push_u32_le(supported_features());
        return (command_id | XNCP_CMD_RESPONSE_BIT, Status::OK);
    }

    match XNCP_COMMANDS.iter().find(|c| c.command_id == command_id) {
        Some(cmd) => (command_id | XNCP_CMD_RESPONSE_BIT, (cmd.handler)(payload, reply)),
        None => (XNCP_CMD_UNKNOWN, Status::NOT_FOUND),
    }
}

/// Returns the total reply length.
fn handle_frame(message: &[u8], reply: &mut [u8]) -> u8 {
    let (header, body) = reply.split_at_mut(REPLY_HEADER_LEN);
    let mut payload = ReplyBuf::new(body);

    let (response_id, status) = dispatch(message, &mut payload);

    let payload_len = payload.len();
    header[0] = (response_id & 0xFF) as u8;
    header[1] = (response_id >> 8) as u8;
    header[2] = status.0;
    (REPLY_HEADER_LEN + payload_len) as u8
}

// Wraps the SDK's weak sl_zigbee_af_xncp_incoming_custom_frame_cb
#[no_mangle]
pub unsafe extern "C" fn __wrap_sl_zigbee_af_xncp_incoming_custom_frame_cb(
    message_length: u8,
    message_payload: *const u8,
    reply_payload_length: *mut u8,
    reply_payload: *mut u8,
) -> u32 {
    let message = core::slice::from_raw_parts(message_payload, message_length as usize);
    let reply = core::slice::from_raw_parts_mut(reply_payload, REPLY_BUF_LEN);
    *reply_payload_length = handle_frame(message, reply);
    Status::OK.0 as u32
}
