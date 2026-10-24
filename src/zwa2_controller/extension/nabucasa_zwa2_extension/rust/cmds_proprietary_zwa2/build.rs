use ohf_bindgen::BuilderExt;

fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("cmd_handler_map_t")
            .allowlist_type("BootloaderInformation_t")
            .allowlist_function("Respond")
            .allowlist_function("bootloader_getInfo")
            .allowlist_var("FUNC_ID_PROPRIETARY_0")
            .allowlist_var("BOOTLOADER_VERSION_(MAJOR|MINOR)_SHIFT")
            .allowlist_var_as("BUF_SIZE_TX", "usize"),
    );
}
