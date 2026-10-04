use std::env;
use std::path::PathBuf;

// All clang flags (SDK includes, defines, sysroot, arch, --target) come from SLC's
// generated project, forwarded by build_project.py as OHF_BINDGEN_FLAGS. A missing/garbage
// flag should crash the build, not degrade it.
fn main() {
    println!("cargo:rerun-if-env-changed=OHF_BINDGEN_FLAGS");
    println!("cargo:rerun-if-changed=wrapper.h");

    let out = PathBuf::from(env::var("OUT_DIR").unwrap());

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
        // SL_ENUM variants (DLK_*) live in a separate `name_enum` type; emit them under
        // their bare C names, like ohf-sys does for the GPIO enums.
        .prepend_enum_name(false)
        .allowlist_type(
            "sl_zigbee_dlk_.*|sl_zigbee_address_info|sl_zigbee_sec_man_.*|sl_status_t",
        )
        .allowlist_function("sl_zigbee_sec_man_init_context|sl_zigbee_sec_man_get_aps_key_info")
        .allowlist_var(
            "DLK_PROTOCOL_.*|DLK_SECRET_.*|SL_ZB_SEC_MAN_KEY_TYPE_TC_LINK_WITH_TIMEOUT|\
             ZB_SEC_MAN_FLAG_EUI_IS_VALID|EUI64_SIZE|\
             SL_STATUS_OK|SL_STATUS_NOT_SUPPORTED|SL_STATUS_NOT_FOUND",
        )
        .clang_macro_fallback()
        .clang_args(&clang_args)
        .generate()
        .expect("bindgen failed");

    bindings.write_to_file(out.join("bindings.rs")).unwrap();
}
