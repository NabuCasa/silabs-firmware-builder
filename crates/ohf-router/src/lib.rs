//! Zigbee router components for the ZBT-2 router firmware. Each is behind a cargo feature
//! matching its `ohf_rust_feature`; SLC enables only the components the manifest adds.
#![no_std]
#![allow(non_camel_case_types, non_upper_case_globals)]

mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

#[cfg(feature = "beacon_filter")]
mod beacon_filter;
#[cfg(feature = "eui64_unique")]
mod eui64_unique;
