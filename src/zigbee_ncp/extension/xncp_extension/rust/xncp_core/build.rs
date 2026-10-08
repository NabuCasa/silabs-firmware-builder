use ohf_bindgen::BuilderExt;

fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_var_as("SL_ZIGBEE_MAX_CUSTOM_EZSP_MESSAGE_PAYLOAD", "usize"),
    );
}
