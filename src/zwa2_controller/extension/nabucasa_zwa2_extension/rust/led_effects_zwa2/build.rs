fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("zpal_status_t")
            .allowlist_function("ZAF_nvm_app_read")
            .allowlist_function("ZAF_nvm_app_write"),
    );
}
