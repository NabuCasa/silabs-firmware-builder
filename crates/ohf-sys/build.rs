use std::env;
use std::path::PathBuf;

fn main() {
    let mut builder = ohf_bindgen::builder()
        .allowlist_type("I2C_TransferSeq_TypeDef")
        .allowlist_type("sl_led_t")
        .allowlist_type("sl_led_rgb_pwm_t")
        .allowlist_type("sl_led_state_t")
        .allowlist_type("sl_status_t")
        .allowlist_type("SPIDRV_Handle_t")
        .allowlist_type("GPIO_Mode_TypeDef")
        .allowlist_type("sl_sleeptimer_timer_handle_t")
        // SL_ENUM variants live in the separate `*_enum` types. Both GPIO port
        // spellings back symbolic config such as WS2812_EN_PORT.
        .allowlist_type("GPIO_Mode_TypeDef_enum")
        .allowlist_type("GPIO_Port_TypeDef_enum")
        .allowlist_type("sl_gpio_port_t_enum")
        .allowlist_function("I2CSPM_Transfer")
        .allowlist_function("sl_udelay_wait")
        .allowlist_function("sl_led_init")
        .allowlist_function("SPIDRV_MTransmit")
        .allowlist_function("sl_led_turn_on")
        .allowlist_function("sl_led_turn_off")
        .allowlist_function("sl_led_set_rgb_color")
        .allowlist_function("sl_sleeptimer_start_periodic_timer_ms")
        .allowlist_function("sl_sleeptimer_stop_timer")
        .allowlist_function("sl_sleeptimer_start_timer")
        .allowlist_function("sl_sleeptimer_ms32_to_tick")
        .allowlist_var("I2C_FLAG_.*")
        .allowlist_var("SL_STATUS_OK")
        .allowlist_var("SL_LED_CURRENT_STATE_.*")
        .opaque_type("SPIDRV_HandleData");

    if std::env::var_os("CARGO_FEATURE_FACTORY_ERASE").is_some() {
        builder = builder
            .clang_arg("-DOHF_FACTORY_ERASE")
            .allowlist_type("nvm3_Handle_t")
            .allowlist_function("nvm3_initDefault")
            .allowlist_function("nvm3_eraseAll")
            .allowlist_function("psa_destroy_key")
            .allowlist_var("nvm3_defaultHandle");
    }

    if std::env::var_os("CARGO_FEATURE_TOKENS").is_some() {
        builder = builder
            .clang_arg("-DOHF_TOKENS")
            .allowlist_function("halInternalGetTokenData")
            .allowlist_type("tokTypeStackNodeData")
            .allowlist_var("TOKEN_STACK_NODE_DATA");
    }

    ohf_bindgen::write(builder);

    println!("cargo:rerun-if-env-changed=OHF_SHIM_CC");
    println!("cargo:rerun-if-env-changed=OHF_SHIM_CFLAGS");
    println!("cargo:rerun-if-changed=shims.c");

    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    let mut cc = cc::Build::new();
    cc.compiler(env::var("OHF_SHIM_CC").expect("OHF_SHIM_CC"));
    cc.file(manifest.join("shims.c"))
        .include(&manifest)
        .include(&out);
    for f in env::var("OHF_SHIM_CFLAGS")
        .expect("OHF_SHIM_CFLAGS")
        .split_whitespace()
    {
        cc.flag(f);
    }
    cc.compile("ohf_sys_shims");
}
