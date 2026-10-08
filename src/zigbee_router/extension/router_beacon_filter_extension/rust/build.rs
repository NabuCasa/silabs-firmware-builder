fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("sl_zigbee_beacon_data_t")
            .allowlist_type("sl_status_t")
            .allowlist_var("SL_STATUS_OK")
            .allowlist_var("SL_STATUS_NOT_FOUND"),
    );
}
