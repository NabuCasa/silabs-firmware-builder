fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            // SL_TOKEN_GET_STATIC_DEVICE_TOKEN is a function-like macro, rebuilt in Rust
            .allowlist_var("SL_TOKEN_TYPE_STATIC_DEVICE")
            .allowlist_var("SL_TOKEN_STATIC_TOKEN_SIZE_OFFSET")
            .allowlist_var("TOKEN_MFG_EUI_64")
            .allowlist_var("TOKEN_MFG_EUI_64_SIZE"),
    );
}
