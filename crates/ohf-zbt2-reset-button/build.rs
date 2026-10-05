fn main() {
    let mut builder = ohf_bindgen::builder()
        .allowlist_type("sl_button_t")
        .allowlist_type("sl_button")
        .allowlist_type("sl_sleeptimer_timer_handle_t")
        .allowlist_type("sl_sleeptimer_timer_callback_t")
        .allowlist_type("sl_status_t")
        .allowlist_function("sl_button_get_state")
        .allowlist_function("sl_sleeptimer_start_timer")
        .allowlist_function("sl_sleeptimer_ms32_to_tick")
        .allowlist_function("sl_sleeptimer_stop_timer")
        .allowlist_var("SL_SIMPLE_BUTTON_PRESSED")
        .allowlist_var("ZBT2_RESET_BUTTON_.*");

    // The RCP build has no zigbee stack headers
    if std::env::var_os("CARGO_FEATURE_ZIGBEE_TOKEN_RESET").is_some() {
        builder = builder
            .clang_arg("-DOHF_ZIGBEE_TOKEN_RESET")
            .allowlist_function("sl_zigbee_token_factory_reset");
    }

    ohf_bindgen::write(builder);
}
