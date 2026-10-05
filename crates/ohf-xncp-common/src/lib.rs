//! Common XNCP command set — commands 0x0001..0x0009, shared by every coordinator.
#![no_std]
#![allow(non_camel_case_types, non_upper_case_globals)]

use core::cell::RefCell;
use core::ffi::c_void;
use core::ptr::addr_of_mut;

use critical_section::Mutex;
use linkme::distributed_slice;

use ohf_xncp_macros::xncp_command;

use ohf_xncp::{
    Reader, ReplyBuf, Status, XNCP_FEATURES,
    XNCP_FEATURE_BUILD_STRING, XNCP_FEATURE_CHIP_INFO, XNCP_FEATURE_COMBINED_SEND,
    XNCP_FEATURE_FLOW_CONTROL_TYPE, XNCP_FEATURE_MANUAL_SOURCE_ROUTE,
    XNCP_FEATURE_MEMBER_OF_ALL_GROUPS, XNCP_FEATURE_MFG_TOKEN_OVERRIDES,
    XNCP_FEATURE_RESTORE_ROUTE_TABLE,
};

mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::*;

const XNCP_SEND_UNICAST_FLAG_EXTENDED_TIMEOUT: u8 = 1 << 0;
const XNCP_SEND_UNICAST_FLAG_SOURCE_ROUTE: u8 = 1 << 1;

const FLOW_CONTROL_TYPE_SOFTWARE: u8 = 0x00;
const FLOW_CONTROL_TYPE_HARDWARE: u8 = 0x01;

// sli_zigbee_route_table_entry_t.status values.
const ROUTE_ACTIVE: u8 = 0;
const ROUTE_UNUSED: u8 = 3;

const RELAY_COUNT: usize = SL_ZIGBEE_MAX_SOURCE_ROUTE_RELAY_COUNT as usize;
const TABLE_SIZE: usize = XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE as usize;

// bindgen emits string macros as `b"..\0"`; strip the trailing NUL to match strlen().
fn c_str_bytes(s: &'static [u8]) -> &'static [u8] {
    &s[..s.len() - 1]
}

// EmberZNet internal tables
extern "C" {
    static mut sli_zigbee_route_table: sli_zigbee_route_table_entry_t;
    static mut sli_zigbee_route_table_size: u8;
    static mut sli_zigbee_address_table_size: u8;
}

fn pseudo_random() -> u16 {
    unsafe { sl_zigbee_get_pseudo_random_number() }
}

fn address_table_size() -> u8 {
    unsafe { sli_zigbee_address_table_size }
}

#[derive(Clone, Copy)]
struct RouteEntry {
    destination: u16,
    next_hop: u16,
    status: u8,
    cost: u8,
}

fn route_table_size() -> u8 {
    unsafe { sli_zigbee_route_table_size }
}

fn route_entry_ptr(index: u8) -> *mut sli_zigbee_route_table_entry_t {
    // The symbol is the first element of the stack's route table array.
    unsafe { addr_of_mut!(sli_zigbee_route_table).add(index as usize) }
}

fn route_table_get(index: u8) -> RouteEntry {
    let e = unsafe { &*route_entry_ptr(index) };
    RouteEntry {
        destination: e.destination,
        next_hop: e.nextHop,
        status: e.status,
        cost: e.cost,
    }
}

fn route_table_set(index: u8, v: RouteEntry) {
    let e = unsafe { &mut *route_entry_ptr(index) };
    e.destination = v.destination;
    e.nextHop = v.next_hop;
    e.status = v.status;
    e.cost = v.cost;
    e.networkIndex = 0;
}

// Picks the slot for a fake route (num_relays == 0 case): an existing entry for `destination`,
// else a free (UNUSED) slot, else a random one.
fn find_free_routing_table_entry(destination: u16) -> u8 {
    let size = route_table_size();
    let mut index: u8 = 0xFF;
    for i in 0..size {
        let e = route_table_get(i);
        if e.destination == destination {
            return i;
        } else if e.status == ROUTE_UNUSED {
            index = i;
        }
    }
    if index == 0xFF {
        index = (pseudo_random() % size as u16) as u8;
    }
    index
}

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

// `relay_bytes` is little-endian u16 relays; its length is validated by the callers.
fn install_manual_source_route(node_id: u16, relay_bytes: &[u8]) {
    let num_relays = (relay_bytes.len() / 2) as u8;
    critical_section::with(|cs| {
        let mut routes = MANUAL_SOURCE_ROUTES.borrow(cs).borrow_mut();

        let mut insertion = pseudo_random() as usize % TABLE_SIZE;
        for i in 0..TABLE_SIZE {
            if !routes[i].active {
                insertion = i;
            } else if routes[i].destination == node_id {
                insertion = i;
                break;
            }
        }

        let route = &mut routes[insertion];
        for (slot, chunk) in route.relays.iter_mut().zip(relay_bytes.chunks_exact(2)) {
            *slot = u16::from_le_bytes([chunk[0], chunk[1]]);
        }
        route.destination = node_id;
        route.num_relays = num_relays;
        route.active = true;
    });
}

#[xncp_command(0x0001)]
fn handle_set_source_route(req: &[u8], _reply: &mut ReplyBuf) -> Status {
    if req.len() < 2 || req.len() % 2 != 0 {
        return Status::BAD_ARGUMENT;
    }
    let relay_bytes = &req[2..];
    if relay_bytes.len() / 2 > RELAY_COUNT {
        return Status::BAD_ARGUMENT;
    }
    let node_id = u16::from_le_bytes([req[0], req[1]]);
    install_manual_source_route(node_id, relay_bytes);
    Status::OK
}

#[xncp_command(0x0002)]
fn handle_get_mfg_token_override(req: &[u8], reply: &mut ReplyBuf) -> Status {
    let &[token_id] = req else {
        return Status::BAD_ARGUMENT;
    };
    let value = if token_id as u32 == SL_ZIGBEE_EZSP_MFG_STRING {
        c_str_bytes(XNCP_MFG_MANUF_NAME)
    } else if token_id as u32 == SL_ZIGBEE_EZSP_MFG_BOARD_NAME {
        c_str_bytes(XNCP_MFG_BOARD_NAME)
    } else {
        return Status::NOT_FOUND;
    };
    reply.push_bytes(value);
    Status::OK
}

#[xncp_command(0x0003)]
fn handle_get_build_string(_req: &[u8], reply: &mut ReplyBuf) -> Status {
    reply.push_bytes(c_str_bytes(XNCP_BUILD_STRING));
    Status::OK
}

#[xncp_command(0x0004)]
fn handle_get_flow_control_type(_req: &[u8], reply: &mut ReplyBuf) -> Status {
    let flow = if XNCP_FLOW_CONTROL_TYPE as u32 == usartHwFlowControlCtsAndRts as u32 {
        FLOW_CONTROL_TYPE_HARDWARE
    } else {
        FLOW_CONTROL_TYPE_SOFTWARE
    };
    reply.push(flow);
    Status::OK
}

#[xncp_command(0x0005)]
fn handle_get_chip_info(_req: &[u8], reply: &mut ReplyBuf) -> Status {
    reply.push_u32_le(RAM_MEM_SIZE as u32);
    let part = c_str_bytes(PART_NUMBER);
    reply.push(part.len() as u8);
    reply.push_bytes(part);
    Status::OK
}

#[xncp_command(0x0006)]
fn handle_set_route_table_entry(req: &[u8], _reply: &mut ReplyBuf) -> Status {
    let &[index, d0, d1, n0, n1, status, cost] = req else {
        return Status::BAD_ARGUMENT;
    };
    if index >= route_table_size() {
        return Status::BAD_ARGUMENT;
    }
    route_table_set(
        index,
        RouteEntry {
            destination: u16::from_le_bytes([d0, d1]),
            next_hop: u16::from_le_bytes([n0, n1]),
            status,
            cost,
        },
    );
    Status::OK
}

#[xncp_command(0x0007)]
fn handle_get_route_table_entry(req: &[u8], reply: &mut ReplyBuf) -> Status {
    let &[index] = req else {
        return Status::BAD_ARGUMENT;
    };
    if index >= route_table_size() {
        return Status::BAD_ARGUMENT;
    }
    let e = route_table_get(index);
    reply.push_u16_le(e.destination);
    reply.push_u16_le(e.next_hop);
    reply.push(e.status);
    reply.push(e.cost);
    Status::OK
}

struct TxPower {
    recommended_dbm: i8,
    max_dbm: i8,
    countries: &'static [[u8; 2]],
}

#[rustfmt::skip]
const TX_POWERS: &[TxPower] = &[
    // RED/ETSI-harmonized region: 10 dBm.
    TxPower {
        recommended_dbm: 10,
        max_dbm: 10,
        countries: &[
            // EU member states
            *b"AT", *b"BE", *b"BG", *b"HR", *b"CY", *b"CZ", *b"DK", *b"EE", *b"FI",
            *b"FR", *b"DE", *b"GR", *b"HU", *b"IE", *b"IT", *b"LV", *b"LT", *b"LU",
            *b"MT", *b"NL", *b"PL", *b"PT", *b"RO", *b"SK", *b"SI", *b"ES", *b"SE",
            // EEA members
            *b"IS", *b"LI", *b"NO",
            // Standards harmonized with RED or ETSI
            *b"CH", *b"GB", *b"TR", *b"AL", *b"BA", *b"GE", *b"MD", *b"ME", *b"MK",
            *b"RS", *b"UA",
            // Other CEPT nations
            *b"AD", *b"AZ", *b"MC", *b"SM", *b"VA",
        ]
    },
    // Testing: a max of 127 effectively disables the cap.
    TxPower {
        recommended_dbm: 8,
        max_dbm: 127,
        countries: &[*b"??"]
    },
];

#[xncp_command(0x0008)]
fn handle_get_tx_power_info(req: &[u8], reply: &mut ReplyBuf) -> Status {
    let Ok(code) = <[u8; 2]>::try_from(req) else {
        return Status::BAD_ARGUMENT;
    };

    let (recommended, max) = TX_POWERS
        .iter()
        .find(|p| p.countries.contains(&code))
        .map(|p| (p.recommended_dbm, p.max_dbm))
        .unwrap_or((
            XNCP_DEFAULT_RECOMMENDED_TX_POWER_DBM as i8,
            XNCP_DEFAULT_MAX_TX_POWER_DBM as i8,
        ));

    reply.push(recommended as u8);
    reply.push(max as u8);
    Status::OK
}

// Like the host's set_extended_timeout: creates an address table entry if none exists
fn apply_extended_timeout(eui64: &[u8], node_id: u16, extended_timeout: bool) {
    // The SDK's eui64 APIs take a non-const pointer but only read these 8 bytes.
    let eui = eui64.as_ptr() as *mut u8;

    let current = unsafe { sl_zigbee_get_extended_timeout(eui) } == SL_STATUS_OK;
    if current == extended_timeout {
        return;
    }

    let mut existing_node_id: u16 = 0;
    let found = unsafe { sl_zigbee_lookup_node_id_by_eui64(eui, &mut existing_node_id) };
    if found == SL_STATUS_OK && existing_node_id != SL_ZIGBEE_TABLE_ENTRY_UNUSED_NODE_ID as u16 {
        unsafe { sl_zigbee_set_extended_timeout(eui, extended_timeout) };
        return;
    }

    // No address table entry exists; replace a random one so the timeout sticks.
    let index = (pseudo_random() % address_table_size() as u16) as u8;
    unsafe {
        sl_zigbee_set_address_table_info(index, eui, node_id);
        sl_zigbee_set_extended_timeout(eui, extended_timeout);
    }
}

fn send_unicast(destination: u16, aps: &mut sl_zigbee_aps_frame_t, tag: u16, message: &[u8]) -> (u32, u8) {
    let mut aps_sequence: u8 = 0;
    let status = unsafe {
        sl_zigbee_send_unicast(
            SL_ZIGBEE_OUTGOING_DIRECT as _,
            destination,
            aps,
            tag,
            message.len() as u8,
            message.as_ptr(),
            &mut aps_sequence,
        )
    };
    (status, aps_sequence)
}

#[xncp_command(0x0009)]
fn handle_send_unicast(req: &[u8], reply: &mut ReplyBuf) -> Status {
    let mut r = Reader::new(req);

    // flags(1) + destination(2) + aps_frame(11) + message_tag(1)
    if r.remaining() < 15 {
        return Status::BAD_ARGUMENT;
    }
    let flags = r.u8().unwrap();
    let destination = r.u16_le().unwrap();
    let mut aps_frame = sl_zigbee_aps_frame_t {
        profileId: r.u16_le().unwrap(),
        clusterId: r.u16_le().unwrap(),
        sourceEndpoint: r.u8().unwrap(),
        destinationEndpoint: r.u8().unwrap(),
        options: r.u16_le().unwrap() as _,
        groupId: r.u16_le().unwrap(),
        sequence: r.u8().unwrap(),
        radius: 0,
    };
    let message_tag = r.u8().unwrap();

    if flags & XNCP_SEND_UNICAST_FLAG_EXTENDED_TIMEOUT != 0 {
        if r.remaining() < 9 {
            return Status::BAD_ARGUMENT;
        }
        let eui64 = r.bytes(8).unwrap();
        let extended_timeout = r.u8().unwrap() != 0;
        apply_extended_timeout(eui64, destination, extended_timeout);
    }

    if flags & XNCP_SEND_UNICAST_FLAG_SOURCE_ROUTE != 0 {
        let Some(num_relays) = r.u8() else {
            return Status::BAD_ARGUMENT;
        };
        let num_relays = num_relays as usize;
        if num_relays > RELAY_COUNT || r.remaining() < num_relays * 2 {
            return Status::BAD_ARGUMENT;
        }
        install_manual_source_route(destination, r.bytes(num_relays * 2).unwrap());
    }

    let (status, aps_sequence) =
        send_unicast(destination, &mut aps_frame, message_tag as u16, r.rest());
    reply.push_u32_le(status);
    reply.push(aps_sequence);
    Status::OK
}

// XNCP_FEATURE_MEMBER_OF_ALL_GROUPS: receive every group's packets
#[no_mangle]
pub extern "C" fn __wrap_sli_zigbee_am_multicast_member(_multicast_id: u16) -> bool {
    true
}

// The `append_to_linked_buffers(buf, data, len)` macro expands to `really_append(&buf, …)`;
// the callback hands us the header pointer, so that `&buf` is the pointer itself.
unsafe fn append_to_header(header: *mut sli_buffer_manager_buffer_t, data: &[u8]) {
    sl_legacy_buffer_manager_really_append_to_linked_buffers(
        header,
        data.as_ptr() as *mut u8,
        data.len() as u16,
        true,
    );
}

// `header` is a sli_buffer_manager_buffer_t* that the buffer manager updates in place
#[no_mangle]
pub unsafe extern "C" fn nc_zigbee_override_append_source_route(
    destination: u16,
    header: *mut c_void,
    consumed: *mut bool,
) {
    let header = header as *mut sli_buffer_manager_buffer_t;

    critical_section::with(|cs| {
        let mut routes = MANUAL_SOURCE_ROUTES.borrow(cs).borrow_mut();

        let Some(idx) = routes
            .iter()
            .position(|r| r.active && r.destination == destination)
        else {
            *consumed = false;
            return;
        };
        *consumed = true;

        // Empty source routes are invalid per the spec: drop a fake route into the stack's
        // route table so EmberZNet just sends the packet directly.
        if routes[idx].num_relays == 0 {
            route_table_set(
                find_free_routing_table_entry(destination),
                RouteEntry {
                    destination,
                    next_hop: destination,
                    status: ROUTE_ACTIVE,
                    cost: 0,
                },
            );
            return;
        }

        let num_relays = routes[idx].num_relays;
        let relay_index = num_relays - 1;
        routes[idx].active = false; // Disable the route after a single use.

        append_to_header(header, &[num_relays]);
        append_to_header(header, &[relay_index]);
        for i in 0..num_relays {
            let relay = routes[idx].relays[(num_relays - i - 1) as usize];
            append_to_header(header, &relay.to_le_bytes());
        }
    });
}

#[distributed_slice(XNCP_FEATURES)]
static COMMON_FEATURES: u32 = XNCP_FEATURE_MEMBER_OF_ALL_GROUPS
    | XNCP_FEATURE_MANUAL_SOURCE_ROUTE
    | XNCP_FEATURE_MFG_TOKEN_OVERRIDES
    | XNCP_FEATURE_BUILD_STRING
    | XNCP_FEATURE_FLOW_CONTROL_TYPE
    | XNCP_FEATURE_CHIP_INFO
    | XNCP_FEATURE_RESTORE_ROUTE_TABLE
    | XNCP_FEATURE_COMBINED_SEND;
