use std::env;
use std::path::PathBuf;

// All clang flags (SDK includes, device define, sysroot, arch, --target) come from SLC's
// generated project, forwarded by build_project.py as OHF_BINDGEN_FLAGS. Nothing about
// the chip or the config is hardcoded here. A missing/garbage flag should crash.
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
        .prepend_enum_name(false)
        .allowlist_type(
            "sl_zigbee_aps_frame_t|sli_zigbee_route_table_entry_t|sl_status_t|\
             USART_HwFlowControl_TypeDef",
        )
        .allowlist_function(
            "sl_zigbee_send_unicast|sl_zigbee_get_extended_timeout|\
             sl_zigbee_set_extended_timeout|sl_zigbee_lookup_node_id_by_eui64|\
             sl_zigbee_set_address_table_info|sl_zigbee_get_pseudo_random_number|\
             sl_legacy_buffer_manager_really_append_to_linked_buffers",
        )
        // Stack constants + the resolved XNCP_* config macros from xncp_config.h.
        .allowlist_var(
            "SL_STATUS_OK|SL_ZIGBEE_OUTGOING_DIRECT|SL_ZIGBEE_TABLE_ENTRY_UNUSED_NODE_ID|\
             SL_ZIGBEE_MAX_SOURCE_ROUTE_RELAY_COUNT|SL_ZIGBEE_EZSP_MFG_.*|\
             RAM_MEM_SIZE|PART_NUMBER|XNCP_.*|usartHwFlowControl.*",
        )
        // Config macros that expand to enum constants / parenthesized casts (e.g.
        // XNCP_FLOW_CONTROL_TYPE, RAM_MEM_SIZE) need a compile to constant-fold.
        .clang_macro_fallback()
        .clang_args(&clang_args)
        .generate()
        .expect("bindgen failed");

    bindings.write_to_file(out.join("bindings.rs")).unwrap();
}
