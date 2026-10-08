use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    let mut builder = ohf_bindgen::builder()
        .allowlist_type("sl_button_t")
        .allowlist_type("sl_button")
        .allowlist_type("sl_status_t")
        .allowlist_function("sl_button_get_state")
        .allowlist_var("SL_SIMPLE_BUTTON_PRESSED");

    // The RCP build has no zigbee stack headers
    if std::env::var_os("CARGO_FEATURE_ZIGBEE_TOKEN_RESET").is_some() {
        builder = builder
            .clang_arg("-DOHF_ZIGBEE_TOKEN_RESET")
            .allowlist_function("sl_zigbee_token_factory_reset");
    }

    ohf_bindgen::write(builder);

    let cfg = ohf_config::Config::load();
    let generated: String = [
        "ZBT2_RESET_BUTTON_CYCLES",
        "ZBT2_RESET_BUTTON_CYCLE_DELAY_MS",
        "ZBT2_RESET_BUTTON_BLINK_ON_MS",
        "ZBT2_RESET_BUTTON_BLINK_OFF_MS",
        "ZBT2_RESET_BUTTON_BLINK_START_DELAY_MS",
    ]
    .iter()
    .map(|key| format!("pub const {key}: u32 = {};\n", cfg.get(key)))
    .collect();

    fs::write(
        PathBuf::from(env::var("OUT_DIR").unwrap()).join("config.rs"),
        generated,
    )
    .unwrap();
}
