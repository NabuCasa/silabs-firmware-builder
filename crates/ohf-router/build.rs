fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("sl_zigbee_beacon_data_t")
            .allowlist_type("sl_status_t")
            .allowlist_var("SL_STATUS_OK")
            .allowlist_var("SL_STATUS_NOT_FOUND")
            // SL_TOKEN_GET_STATIC_DEVICE_TOKEN is a function-like macro, rebuilt in Rust
            .allowlist_var("SL_TOKEN_TYPE_STATIC_DEVICE")
            .allowlist_var("SL_TOKEN_STATIC_TOKEN_SIZE_OFFSET")
            .allowlist_var("TOKEN_MFG_EUI_64")
            .allowlist_var("TOKEN_MFG_EUI_64_SIZE")
            .allowlist_var("SL_ZIGBEE_ROUTER")
            .allowlist_var("SL_ZIGBEE_UNKNOWN_DEVICE"),
    );
}
