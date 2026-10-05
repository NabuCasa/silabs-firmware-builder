// Clang flags come from SLC's generated project via OHF_BINDGEN_FLAGS. Shared bindgen setup
// is in ohf-bindgen; only this crate's allowlist is stated here.
fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("sl_zigbee_beacon_data_t")
            .allowlist_type("sl_status_t")
            .allowlist_var("SL_STATUS_OK")
            .allowlist_var("SL_STATUS_NOT_FOUND")
            // Parts of the manufacturing EUI64 token id (SL_TOKEN_GET_STATIC_DEVICE_TOKEN is
            // function-like, so compute it in Rust from these).
            .allowlist_var("SL_TOKEN_TYPE_STATIC_DEVICE")
            .allowlist_var("SL_TOKEN_STATIC_TOKEN_SIZE_OFFSET")
            .allowlist_var("TOKEN_MFG_EUI_64")
            .allowlist_var("TOKEN_MFG_EUI_64_SIZE")
            // Node types for the nvram-reset check.
            .allowlist_var("SL_ZIGBEE_ROUTER")
            .allowlist_var("SL_ZIGBEE_UNKNOWN_DEVICE")
            // Install-code provisioning: Zigbee CRC + PSA randomness.
            .allowlist_function("halCommonCrc16")
            .allowlist_function("psa_crypto_init")
            .allowlist_function("psa_generate_random")
            .allowlist_var("PSA_SUCCESS"),
    );
}
