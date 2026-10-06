//! Zigbee router components, one cargo feature each.
#![no_std]

#[allow(
    non_camel_case_types,
    non_upper_case_globals,
    non_snake_case,
    dead_code
)]
mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

#[cfg(feature = "beacon_filter")]
mod beacon_filter;
#[cfg(feature = "eui64_unique")]
mod eui64_unique;
#[cfg(feature = "nvram_reset")]
mod nvram_reset;
