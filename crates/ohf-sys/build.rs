use std::env;
use std::path::PathBuf;

// All clang/cc flags (SDK includes, device define, sysroot, arch, --target) come from
// SLC's generated project, forwarded by build_project.py. Nothing is hardcoded here.
// Missing/garbage flags should crash the build, not degrade it.
fn main() {
    println!("cargo:rerun-if-env-changed=OHF_BINDGEN_FLAGS");
    println!("cargo:rerun-if-env-changed=OHF_SHIM_CC");
    println!("cargo:rerun-if-env-changed=OHF_SHIM_CFLAGS");
    println!("cargo:rerun-if-changed=wrapper.h");
    println!("cargo:rerun-if-changed=shims.c");

    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());

    let clang_args: Vec<String> = env::var("OHF_BINDGEN_FLAGS")
        .expect("OHF_BINDGEN_FLAGS (SLC-derived clang args)")
        .split_whitespace()
        .map(str::to_owned)
        .collect();

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .use_core()
        .layout_tests(false)
        .generate_comments(false)
        // Emit enum variants under their C names (gpioModePushPull), not
        // EnumName_variant.
        .prepend_enum_name(false)
        .allowlist_type(
            "I2C_TransferSeq_TypeDef|sl_led_t|sl_led_rgb_pwm_t|sl_led_state_t|sl_status_t|\
             SPIDRV_Handle_t|GPIO_Mode_TypeDef|sl_sleeptimer_timer_handle_t|CORE_irqState_t",
        )
        .allowlist_function(
            "I2CSPM_Transfer|sl_udelay_wait|sl_led_init|SPIDRV_MTransmit|\
             sl_led_turn_on|sl_led_turn_off|sl_led_set_rgb_color|\
             sl_sleeptimer_start_periodic_timer_ms|sl_sleeptimer_stop_timer|\
             CORE_EnterCritical|CORE_ExitCritical",
        )
        .allowlist_var("I2C_FLAG_.*|SL_STATUS_OK|SL_LED_CURRENT_STATE_.*")
        // SL_ENUM[_GENERIC] expands to `typedef T name; enum name##_enum {...}`, so the
        // variants (gpioMode*, SL_GPIO_PORT_*) live in the separate `_enum` type, which
        // must be allowlisted by name. These back symbol-valued config (WS2812_EN_PORT).
        // Both GPIO port spellings a manifest might use for a *_EN_PORT config:
        // SL_GPIO_PORT_* (sl_device_gpio) and gpioPort* (emlib GPIO_Port_TypeDef).
        .allowlist_type("GPIO_Mode_TypeDef_enum|GPIO_Port_TypeDef_enum|sl_gpio_port_t_enum")
        .opaque_type("SPIDRV_HandleData")
        // SL_STATUS_OK etc. are cast macros bindgen can't constant-fold alone.
        .clang_macro_fallback()
        .clang_args(&clang_args)
        .generate()
        .expect("bindgen failed");

    bindings.write_to_file(out.join("bindings.rs")).unwrap();

    // Compile the generic inline-SDK wrappers (shims.c) into ohf-sys, so the real
    // symbols they expose for __STATIC_INLINE helpers (GPIO_PinModeSet, …) bundle into
    // the staticlib. gcc + the SDK arch/include flags, not clang.
    let mut cc = cc::Build::new();
    cc.compiler(env::var("OHF_SHIM_CC").expect("OHF_SHIM_CC"));
    cc.file(manifest.join("shims.c")).include(&manifest).include(&out);
    for f in env::var("OHF_SHIM_CFLAGS")
        .expect("OHF_SHIM_CFLAGS")
        .split_whitespace()
    {
        cc.flag(f);
    }
    cc.compile("ohf_sys_shims");
}
