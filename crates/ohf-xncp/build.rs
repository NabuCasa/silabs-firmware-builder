use ohf_bindgen::BuilderExt;

fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("sl_status_t")
            .allowlist_var("SL_STATUS_OK")
            .allowlist_var("SL_STATUS_INVALID_PARAMETER")
            .allowlist_var("SL_STATUS_NOT_FOUND")
            .allowlist_var_as("SL_ZIGBEE_MAX_CUSTOM_EZSP_MESSAGE_PAYLOAD", "usize"),
    );
}
