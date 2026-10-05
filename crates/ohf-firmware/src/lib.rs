//! Links the enabled components into one staticlib.
#![no_std]

// The critical-section impl
extern crate ohf_rt;

#[cfg(feature = "led_effects")]
extern crate ohf_led_effects;
#[cfg(feature = "led_effects_openthread")]
extern crate ohf_led_effects_openthread;
#[cfg(feature = "led_effects_zigbee")]
extern crate ohf_led_effects_zigbee;
#[cfg(feature = "ncp_dlk_policy")]
extern crate ohf_ncp_dlk_policy;
#[cfg(feature = "qma6100p")]
extern crate ohf_qma6100p;
#[cfg(any(
    feature = "router_beacon_filter",
    feature = "router_eui64_unique",
    feature = "router_nvram_reset",
    feature = "router_install_code"
))]
extern crate ohf_router;
#[cfg(feature = "ws2812")]
extern crate ohf_ws2812;
#[cfg(feature = "xncp_core")]
extern crate ohf_xncp;
#[cfg(feature = "xncp_common")]
extern crate ohf_xncp_common;
#[cfg(feature = "xncp_zbt2")]
extern crate ohf_xncp_zbt2;
#[cfg(feature = "zbt2_reset_button")]
extern crate ohf_zbt2_reset_button;
#[cfg(feature = "zbt2_router_callbacks")]
extern crate ohf_zbt2_router;
