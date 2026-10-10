fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("sl_cli_command_arg_t")
            .allowlist_type("BootloaderInformation_t")
            .allowlist_function("bootloader_getInfo")
            .allowlist_function("bootloader_rebootAndInstall")
            // sl_zigbee_af_app_print is a macro over this
            .allowlist_function("sl_zigbee_af_print")
            .allowlist_var("SL_ZIGBEE_DEBUG_ZCL_GROUP_ENABLED"),
    );
}
