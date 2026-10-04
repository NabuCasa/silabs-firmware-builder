//! Common XNCP command set — Rust port of `xncp_common_commands.c` + `tx_power.c`.
//!
//! Commands 0x0001..0x0009, shared by every coordinator. Registers into the XNCP core's
//! linkme slices (`XNCP_COMMANDS` / `XNCP_FEATURES`); SLC decides whether this crate is
//! linked (feature `xncp_common`). The wire protocol is preserved byte-for-byte.
//!
//! This set reaches deep into the EmberZNet stack: the internal route table
//! (`sli_zigbee_route_table`), the legacy buffer manager, and `sl_zigbee_send_unicast`.
//! Those bindings are generated in this crate's own build.rs — they can't live in the
//! shared ohf-sys, which is also compiled for the non-zigbee OpenThread RCP.
//!
//! `ezsp_version.c` stays C: it's a direct-object data override of the stack's
//! `sl_zigbee_version`, not a command.
#![no_std]
#![allow(non_camel_case_types, non_upper_case_globals)]

use core::cell::RefCell;
use core::ffi::c_void;
use core::ptr::addr_of_mut;

use critical_section::Mutex;
use linkme::distributed_slice;

use ohf_xncp::{
    XncpCommandDef, XncpContext, XNCP_COMMANDS, XNCP_FEATURES, XNCP_FEATURE_BUILD_STRING,
    XNCP_FEATURE_CHIP_INFO, XNCP_FEATURE_COMBINED_SEND, XNCP_FEATURE_FLOW_CONTROL_TYPE,
    XNCP_FEATURE_MANUAL_SOURCE_ROUTE, XNCP_FEATURE_MEMBER_OF_ALL_GROUPS,
    XNCP_FEATURE_MFG_TOKEN_OVERRIDES, XNCP_FEATURE_RESTORE_ROUTE_TABLE,
};

mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::*;

// Low byte of the sl_status_t the wire protocol carries (xncp_core truncates to u8).
const XNCP_STATUS_OK: u8 = 0x00;
const XNCP_STATUS_BAD_ARGUMENT: u8 = 0x21; // SL_STATUS_INVALID_PARAMETER
const XNCP_STATUS_NOT_FOUND: u8 = 0x25; // SL_STATUS_NOT_FOUND

const XNCP_SEND_UNICAST_FLAG_EXTENDED_TIMEOUT: u8 = 1 << 0;
const XNCP_SEND_UNICAST_FLAG_SOURCE_ROUTE: u8 = 1 << 1;

// Flow control types reported to the host (handle_get_flow_control_type).
const FLOW_CONTROL_TYPE_SOFTWARE: u8 = 0x00;
const FLOW_CONTROL_TYPE_HARDWARE: u8 = 0x01;

// sli_zigbee_route_table_entry_t.status values.
const ROUTE_ACTIVE: u8 = 0;
const ROUTE_UNUSED: u8 = 3;

const RELAY_COUNT: usize = SL_ZIGBEE_MAX_SOURCE_ROUTE_RELAY_COUNT as usize;
const TABLE_SIZE: usize = XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE as usize;

// --- EmberZNet internal tables (data symbols, not emitted by bindgen) ----------------
extern "C" {
    static mut sli_zigbee_route_table: sli_zigbee_route_table_entry_t;
    static mut sli_zigbee_route_table_size: u8;
    static mut sli_zigbee_address_table_size: u8;
}

// --- Manual source routes (XNCP_FEATURE_MANUAL_SOURCE_ROUTE) -------------------------
#[derive(Clone, Copy)]
struct ManualSourceRoute {
    active: bool,
    destination: u16,
    num_relays: u8,
    relays: [u16; RELAY_COUNT],
}

const EMPTY_ROUTE: ManualSourceRoute = ManualSourceRoute {
    active: false,
    destination: 0,
    num_relays: 0,
    relays: [0; RELAY_COUNT],
};

static MANUAL_SOURCE_ROUTES: Mutex<RefCell<[ManualSourceRoute; TABLE_SIZE]>> =
    Mutex::new(RefCell::new([EMPTY_ROUTE; TABLE_SIZE]));

#[inline]
fn build_u16(low: u8, high: u8) -> u16 {
    u16::from_le_bytes([low, high])
}

unsafe fn push(ctx: &mut XncpContext, byte: u8) {
    let off = *ctx.reply_length as usize;
    *ctx.reply.add(off) = byte;
    *ctx.reply_length += 1;
}

unsafe fn push_bytes(ctx: &mut XncpContext, src: &[u8]) {
    let off = *ctx.reply_length as usize;
    core::ptr::copy_nonoverlapping(src.as_ptr(), ctx.reply.add(off), src.len());
    *ctx.reply_length += src.len() as u8;
}

// bindgen emits string macros as `b"..\0"`; strip the trailing NUL to match strlen().
fn c_str_bytes(s: &'static [u8]) -> &'static [u8] {
    &s[..s.len() - 1]
}

fn install_manual_source_route(node_id: u16, relay_bytes: *const u8, num_relays: u8) {
    critical_section::with(|cs| {
        let mut routes = MANUAL_SOURCE_ROUTES.borrow(cs).borrow_mut();

        let mut insertion = unsafe { sl_zigbee_get_pseudo_random_number() } as usize % TABLE_SIZE;
        for i in 0..TABLE_SIZE {
            if !routes[i].active {
                insertion = i;
            } else if routes[i].destination == node_id {
                insertion = i;
                break;
            }
        }

        let route = &mut routes[insertion];
        for i in 0..num_relays as usize {
            route.relays[i] =
                unsafe { build_u16(*relay_bytes.add(2 * i), *relay_bytes.add(2 * i + 1)) };
        }
        route.destination = node_id;
        route.num_relays = num_relays;
        route.active = true;
    });
}

// --- Commands -----------------------------------------------------------------------

unsafe extern "C" fn handle_set_source_route(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;

    if ctx.payload_length < 2 || ctx.payload_length % 2 != 0 {
        *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
        return true;
    }

    let num_relays = (ctx.payload_length - 2) / 2;
    if num_relays as usize > RELAY_COUNT {
        *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
        return true;
    }

    let node_id = build_u16(*ctx.payload, *ctx.payload.add(1));
    install_manual_source_route(node_id, ctx.payload.add(2), num_relays);

    *ctx.status = XNCP_STATUS_OK;
    true
}

unsafe extern "C" fn handle_get_mfg_token_override(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;

    if ctx.payload_length != 1 {
        *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
        return true;
    }

    let token_id = *ctx.payload as u32;
    let value = if token_id == SL_ZIGBEE_EZSP_MFG_STRING {
        c_str_bytes(XNCP_MFG_MANUF_NAME)
    } else if token_id == SL_ZIGBEE_EZSP_MFG_BOARD_NAME {
        c_str_bytes(XNCP_MFG_BOARD_NAME)
    } else {
        *ctx.status = XNCP_STATUS_NOT_FOUND;
        return true;
    };

    push_bytes(ctx, value);
    *ctx.status = XNCP_STATUS_OK;
    true
}

unsafe extern "C" fn handle_get_build_string(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;
    push_bytes(ctx, c_str_bytes(XNCP_BUILD_STRING));
    *ctx.status = XNCP_STATUS_OK;
    true
}

unsafe extern "C" fn handle_get_flow_control_type(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;

    let flow = if XNCP_FLOW_CONTROL_TYPE as u32 == usartHwFlowControlCtsAndRts as u32 {
        FLOW_CONTROL_TYPE_HARDWARE
    } else {
        FLOW_CONTROL_TYPE_SOFTWARE
    };

    push(ctx, flow);
    *ctx.status = XNCP_STATUS_OK;
    true
}

unsafe extern "C" fn handle_get_chip_info(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;

    push_bytes(ctx, &(RAM_MEM_SIZE as u32).to_le_bytes());

    let part = c_str_bytes(PART_NUMBER);
    push(ctx, part.len() as u8);
    push_bytes(ctx, part);

    *ctx.status = XNCP_STATUS_OK;
    true
}

// --- Route table management (XNCP_FEATURE_RESTORE_ROUTE_TABLE) -----------------------

unsafe extern "C" fn handle_set_route_table_entry(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;

    if ctx.payload_length != 7 {
        *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
        return true;
    }

    let index = *ctx.payload;
    if index >= sli_zigbee_route_table_size {
        *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
        return true;
    }

    let entry = addr_of_mut!(sli_zigbee_route_table).add(index as usize);
    (*entry).destination = build_u16(*ctx.payload.add(1), *ctx.payload.add(2));
    (*entry).nextHop = build_u16(*ctx.payload.add(3), *ctx.payload.add(4));
    (*entry).status = *ctx.payload.add(5);
    (*entry).cost = *ctx.payload.add(6);
    (*entry).networkIndex = 0;

    *ctx.status = XNCP_STATUS_OK;
    true
}

unsafe extern "C" fn handle_get_route_table_entry(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;

    if ctx.payload_length != 1 {
        *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
        return true;
    }

    let index = *ctx.payload;
    if index >= sli_zigbee_route_table_size {
        *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
        return true;
    }

    let entry = addr_of_mut!(sli_zigbee_route_table).add(index as usize);
    push_bytes(ctx, &(*entry).destination.to_le_bytes());
    push_bytes(ctx, &(*entry).nextHop.to_le_bytes());
    push(ctx, (*entry).status);
    push(ctx, (*entry).cost);

    *ctx.status = XNCP_STATUS_OK;
    true
}

// --- TX power info (XNCP_FEATURE_TX_POWER_INFO is contributed by the ZBT-2 set) ------
// Country-specific TX power table, ported from tx_power.c. Returned by 0x0008.
static COUNTRY_TX_POWERS: &[(u8, u8, i8, i8)] = &[
    // EU Member States
    (b'A', b'T', 10, 10),
    (b'B', b'E', 10, 10),
    (b'B', b'G', 10, 10),
    (b'H', b'R', 10, 10),
    (b'C', b'Y', 10, 10),
    (b'C', b'Z', 10, 10),
    (b'D', b'K', 10, 10),
    (b'E', b'E', 10, 10),
    (b'F', b'I', 10, 10),
    (b'F', b'R', 10, 10),
    (b'D', b'E', 10, 10),
    (b'G', b'R', 10, 10),
    (b'H', b'U', 10, 10),
    (b'I', b'E', 10, 10),
    (b'I', b'T', 10, 10),
    (b'L', b'V', 10, 10),
    (b'L', b'T', 10, 10),
    (b'L', b'U', 10, 10),
    (b'M', b'T', 10, 10),
    (b'N', b'L', 10, 10),
    (b'P', b'L', 10, 10),
    (b'P', b'T', 10, 10),
    (b'R', b'O', 10, 10),
    (b'S', b'K', 10, 10),
    (b'S', b'I', 10, 10),
    (b'E', b'S', 10, 10),
    (b'S', b'E', 10, 10),
    // EEA Members
    (b'I', b'S', 10, 10),
    (b'L', b'I', 10, 10),
    (b'N', b'O', 10, 10),
    // Standards harmonized with RED or ETSI
    (b'C', b'H', 10, 10),
    (b'G', b'B', 10, 10),
    (b'T', b'R', 10, 10),
    (b'A', b'L', 10, 10),
    (b'B', b'A', 10, 10),
    (b'G', b'E', 10, 10),
    (b'M', b'D', 10, 10),
    (b'M', b'E', 10, 10),
    (b'M', b'K', 10, 10),
    (b'R', b'S', 10, 10),
    (b'U', b'A', 10, 10),
    // Other CEPT nations
    (b'A', b'D', 10, 10),
    (b'A', b'Z', 10, 10),
    (b'M', b'C', 10, 10),
    (b'S', b'M', 10, 10),
    (b'V', b'A', 10, 10),
    // Disable the maximum, for testing
    (b'?', b'?', 8, 127),
];

fn get_tx_power_for_country(c1: u8, c2: u8) -> (i8, i8) {
    for &(a, b, recommended, max) in COUNTRY_TX_POWERS {
        if a == c1 && b == c2 {
            return (recommended, max);
        }
    }
    (
        XNCP_DEFAULT_RECOMMENDED_TX_POWER_DBM as i8,
        XNCP_DEFAULT_MAX_TX_POWER_DBM as i8,
    )
}

unsafe extern "C" fn handle_get_tx_power_info(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;

    if ctx.payload_length != 2 {
        *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
        return true;
    }

    let (recommended, max) = get_tx_power_for_country(*ctx.payload, *ctx.payload.add(1));
    *ctx.status = XNCP_STATUS_OK;
    push(ctx, recommended as u8);
    push(ctx, max as u8);
    true
}

// --- Combined send (XNCP_FEATURE_COMBINED_SEND) -------------------------------------

// Mirrors the host set_extended_timeout logic locally: skip if already set, otherwise
// set it, creating an address table entry first if none exists.
unsafe fn apply_extended_timeout(eui64: *mut u8, node_id: u16, extended_timeout: bool) {
    let current = sl_zigbee_get_extended_timeout(eui64) == SL_STATUS_OK;
    if current == extended_timeout {
        return;
    }

    let mut existing_node_id: u16 = 0;
    if sl_zigbee_lookup_node_id_by_eui64(eui64, &mut existing_node_id) == SL_STATUS_OK
        && existing_node_id != SL_ZIGBEE_TABLE_ENTRY_UNUSED_NODE_ID as u16
    {
        sl_zigbee_set_extended_timeout(eui64, extended_timeout);
        return;
    }

    // No address table entry exists; replace a random one so the timeout sticks.
    let index = (sl_zigbee_get_pseudo_random_number() % sli_zigbee_address_table_size as u16) as u8;
    sl_zigbee_set_address_table_info(index, eui64, node_id);
    sl_zigbee_set_extended_timeout(eui64, extended_timeout);
}

unsafe extern "C" fn handle_send_unicast(ctx: *mut XncpContext) -> bool {
    let ctx = &mut *ctx;

    let mut p = ctx.payload;
    let end = ctx.payload.add(ctx.payload_length as usize);
    let remaining = |p: *const u8| end as isize - p as isize;

    // flags(1) + destination(2) + aps_frame(11) + message_tag(1)
    if remaining(p) < 15 {
        *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
        return true;
    }

    let flags = *p;
    p = p.add(1);

    let destination = build_u16(*p, *p.add(1));
    p = p.add(2);

    let mut aps_frame: sl_zigbee_aps_frame_t = core::mem::zeroed();
    aps_frame.profileId = build_u16(*p, *p.add(1));
    p = p.add(2);
    aps_frame.clusterId = build_u16(*p, *p.add(1));
    p = p.add(2);
    aps_frame.sourceEndpoint = *p;
    p = p.add(1);
    aps_frame.destinationEndpoint = *p;
    p = p.add(1);
    aps_frame.options = build_u16(*p, *p.add(1)) as _;
    p = p.add(2);
    aps_frame.groupId = build_u16(*p, *p.add(1));
    p = p.add(2);
    aps_frame.sequence = *p;
    p = p.add(1);
    aps_frame.radius = 0;

    let message_tag = *p;
    p = p.add(1);

    if flags & XNCP_SEND_UNICAST_FLAG_EXTENDED_TIMEOUT != 0 {
        if remaining(p) < 9 {
            *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
            return true;
        }
        let eui64 = p;
        p = p.add(8);
        let extended_timeout = *p != 0;
        p = p.add(1);
        apply_extended_timeout(eui64, destination, extended_timeout);
    }

    if flags & XNCP_SEND_UNICAST_FLAG_SOURCE_ROUTE != 0 {
        if remaining(p) < 1 {
            *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
            return true;
        }
        let num_relays = *p;
        p = p.add(1);
        if num_relays as usize > RELAY_COUNT || remaining(p) < (num_relays as isize * 2) {
            *ctx.status = XNCP_STATUS_BAD_ARGUMENT;
            return true;
        }
        install_manual_source_route(destination, p, num_relays);
        p = p.add(num_relays as usize * 2);
    }

    let message_length = remaining(p) as u8;

    let mut aps_sequence: u8 = 0;
    let status = sl_zigbee_send_unicast(
        SL_ZIGBEE_OUTGOING_DIRECT as _,
        destination,
        &mut aps_frame,
        message_tag as u16,
        message_length,
        p,
        &mut aps_sequence,
    );

    push_bytes(ctx, &(status as u32).to_le_bytes());
    push(ctx, aps_sequence);

    *ctx.status = XNCP_STATUS_OK;
    true
}

// --- Stack callbacks ----------------------------------------------------------------

// SDK init callback (registered via zigbee_af_callback event_init). The static table is
// already zero-initialized (active = false); kept to own the registered symbol and match
// the C init exactly.
#[no_mangle]
pub extern "C" fn xncp_common_init(_init_level: u8) {
    critical_section::with(|cs| {
        let mut routes = MANUAL_SOURCE_ROUTES.borrow(cs).borrow_mut();
        for route in routes.iter_mut() {
            route.active = false;
        }
    });
}

// Multicast override (XNCP_FEATURE_MEMBER_OF_ALL_GROUPS). Pulled in by the linker's
// --wrap=sli_zigbee_am_multicast_member (toolchain_settings in the slcc). We want all
// group packets, so ignore binding/multicast table logic.
#[no_mangle]
pub extern "C" fn __wrap_sli_zigbee_am_multicast_member(_multicast_id: u16) -> bool {
    true
}

// Picks the route table slot to overwrite with a fake route (num_relays == 0 case):
// reuse an entry already bound to `destination`, else a free (UNUSED) slot, else a
// random one. Operates on the stack's own route table, not our manual table.
unsafe fn find_free_routing_table_entry(destination: u16) -> *mut sli_zigbee_route_table_entry_t {
    let size = sli_zigbee_route_table_size;
    let base = addr_of_mut!(sli_zigbee_route_table);
    let mut index: u8 = 0xFF;

    for i in 0..size {
        let entry = base.add(i as usize);
        if (*entry).destination == destination {
            return entry;
        } else if (*entry).status == ROUTE_UNUSED {
            index = i;
        }
    }

    if index == 0xFF {
        index = (sl_zigbee_get_pseudo_random_number() % size as u16) as u8;
    }

    base.add(index as usize)
}

// Source-route override (XNCP_FEATURE_MANUAL_SOURCE_ROUTE), registered via the
// zigbee_stack_callback override_append_source_route contribution. `header` is really an
// sli_buffer_manager_buffer_t* (uint16_t*) that the buffer manager updates in place.
#[no_mangle]
pub unsafe extern "C" fn nc_zigbee_override_append_source_route(
    destination: u16,
    header: *mut c_void,
    consumed: *mut bool,
) {
    let header = header as *mut sli_buffer_manager_buffer_t;

    critical_section::with(|cs| {
        let mut routes = MANUAL_SOURCE_ROUTES.borrow(cs).borrow_mut();

        let idx = routes
            .iter()
            .position(|r| r.active && r.destination == destination);
        let Some(idx) = idx else {
            *consumed = false;
            return;
        };

        *consumed = true;

        // Empty source routes are invalid per the spec: instead drop a fake route into
        // the stack's route table so EmberZNet just sends the packet directly.
        if routes[idx].num_relays == 0 {
            let entry = find_free_routing_table_entry(destination);
            (*entry).destination = destination;
            (*entry).nextHop = destination;
            (*entry).status = ROUTE_ACTIVE;
            (*entry).cost = 0;
            (*entry).networkIndex = 0;
            return;
        }

        let num_relays = routes[idx].num_relays;
        let relay_index = num_relays - 1;

        routes[idx].active = false; // Disable the route after a single use.

        append_to_header(header, &num_relays, 1);
        append_to_header(header, &relay_index, 1);

        for i in 0..num_relays {
            let relay = routes[idx].relays[(num_relays - i - 1) as usize];
            append_to_header(header, relay.to_le_bytes().as_ptr(), 2);
        }
    });
}

// The `sl_legacy_buffer_manager_append_to_linked_buffers(buf, data, len)` macro expands
// to `really_append(&buf, data, len, true)`; the callback hands us the header pointer, so
// that `&buf` is the pointer itself.
#[inline]
unsafe fn append_to_header(header: *mut sli_buffer_manager_buffer_t, data: *const u8, len: u16) {
    sl_legacy_buffer_manager_really_append_to_linked_buffers(header, data as *mut u8, len, true);
}

// --- Registration -------------------------------------------------------------------

#[distributed_slice(XNCP_COMMANDS)]
static CMD_SET_SOURCE_ROUTE: XncpCommandDef =
    XncpCommandDef { command_id: 0x0001, handler: Some(handle_set_source_route) };
#[distributed_slice(XNCP_COMMANDS)]
static CMD_GET_MFG_TOKEN: XncpCommandDef =
    XncpCommandDef { command_id: 0x0002, handler: Some(handle_get_mfg_token_override) };
#[distributed_slice(XNCP_COMMANDS)]
static CMD_GET_BUILD_STRING: XncpCommandDef =
    XncpCommandDef { command_id: 0x0003, handler: Some(handle_get_build_string) };
#[distributed_slice(XNCP_COMMANDS)]
static CMD_GET_FLOW_CONTROL: XncpCommandDef =
    XncpCommandDef { command_id: 0x0004, handler: Some(handle_get_flow_control_type) };
#[distributed_slice(XNCP_COMMANDS)]
static CMD_GET_CHIP_INFO: XncpCommandDef =
    XncpCommandDef { command_id: 0x0005, handler: Some(handle_get_chip_info) };
#[distributed_slice(XNCP_COMMANDS)]
static CMD_SET_ROUTE_TABLE_ENTRY: XncpCommandDef =
    XncpCommandDef { command_id: 0x0006, handler: Some(handle_set_route_table_entry) };
#[distributed_slice(XNCP_COMMANDS)]
static CMD_GET_ROUTE_TABLE_ENTRY: XncpCommandDef =
    XncpCommandDef { command_id: 0x0007, handler: Some(handle_get_route_table_entry) };
#[distributed_slice(XNCP_COMMANDS)]
static CMD_GET_TX_POWER_INFO: XncpCommandDef =
    XncpCommandDef { command_id: 0x0008, handler: Some(handle_get_tx_power_info) };
#[distributed_slice(XNCP_COMMANDS)]
static CMD_SEND_UNICAST: XncpCommandDef =
    XncpCommandDef { command_id: 0x0009, handler: Some(handle_send_unicast) };

#[distributed_slice(XNCP_FEATURES)]
static COMMON_FEATURES: u32 = XNCP_FEATURE_MEMBER_OF_ALL_GROUPS
    | XNCP_FEATURE_MANUAL_SOURCE_ROUTE
    | XNCP_FEATURE_MFG_TOKEN_OVERRIDES
    | XNCP_FEATURE_BUILD_STRING
    | XNCP_FEATURE_FLOW_CONTROL_TYPE
    | XNCP_FEATURE_CHIP_INFO
    | XNCP_FEATURE_RESTORE_ROUTE_TABLE
    | XNCP_FEATURE_COMBINED_SEND;
