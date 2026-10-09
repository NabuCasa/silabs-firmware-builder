fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_var("SL_ZIGBEE_ROUTER")
            .allowlist_var("SL_ZIGBEE_UNKNOWN_DEVICE"),
    );
}
