use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("sl_zigbee_aps_frame_t")
            .allowlist_type("sli_zigbee_route_table_entry_t")
            .allowlist_type("sl_status_t")
            .allowlist_type("sl_zigbee_version_t")
            .allowlist_type("sl_iostream_eusart_uart_flow_control_t_enum")
            .allowlist_type("USART_HwFlowControl_TypeDef")
            .allowlist_function("sl_zigbee_send_unicast")
            .allowlist_function("sl_zigbee_get_extended_timeout")
            .allowlist_function("sl_zigbee_set_extended_timeout")
            .allowlist_function("sl_zigbee_lookup_node_id_by_eui64")
            .allowlist_function("sl_zigbee_set_address_table_info")
            .allowlist_function("sl_zigbee_get_pseudo_random_number")
            .allowlist_function("sl_legacy_buffer_manager_really_append_to_linked_buffers")
            .allowlist_var("SL_STATUS_OK")
            .allowlist_var("SL_ZIGBEE_OUTGOING_DIRECT")
            .allowlist_var("SL_ZIGBEE_TABLE_ENTRY_UNUSED_NODE_ID")
            .allowlist_var_as("SL_ZIGBEE_MAX_SOURCE_ROUTE_RELAY_COUNT", "usize")
            .allowlist_var("SL_ZIGBEE_EZSP_MFG_.*")
            .allowlist_var("RAM_MEM_SIZE")
            .allowlist_var("PART_NUMBER")
            .allowlist_var("SL_ZIGBEE_(BUILD_NUMBER|MAJOR_VERSION|MINOR_VERSION|PATCH_VERSION)")
            .allowlist_var("SL_ZIGBEE_(SPECIAL_VERSION|VERSION_TYPE)")
            .allowlist_var("OHF_VCOM_FLOW_CONTROL_TYPE")
            .allowlist_var("usartHwFlowControl.*"),
    );

    let cfg = ohf_config::Config::load("xncp_common");

    // Symbolic values (e.g. SL_IOSTREAM_EUSART_UART_FLOW_CTRL_NONE) come from the bindings
    let expr = |key: &str| {
        let v = cfg.get(key);
        if v.starts_with(|c: char| c.is_ascii_digit() || c == '-') {
            v.to_owned()
        } else {
            format!("bindings::{v}")
        }
    };

    let byte_str = |key: &str| {
        let escaped: String = cfg
            .get_bytes(key)
            .into_iter()
            .flat_map(std::ascii::escape_default)
            .map(char::from)
            .collect();
        format!("b\"{escaped}\"")
    };

    let generated = format!(
        "pub const XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE: usize = {} as usize;\n\
         pub const XNCP_MFG_MANUF_NAME: &[u8] = {};\n\
         pub const XNCP_MFG_BOARD_NAME: &[u8] = {};\n\
         pub const XNCP_BUILD_STRING: &[u8] = {};\n\
         pub const XNCP_FLOW_CONTROL_TYPE: u32 = {} as u32;\n\
         pub const XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE: u8 = {} as u8;\n\
         pub const XNCP_DEFAULT_RECOMMENDED_TX_POWER_DBM: i8 = {} as i8;\n\
         pub const XNCP_DEFAULT_MAX_TX_POWER_DBM: i8 = {} as i8;\n",
        expr("XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE"),
        byte_str("XNCP_MFG_MANUF_NAME"),
        byte_str("XNCP_MFG_BOARD_NAME"),
        byte_str("XNCP_BUILD_STRING"),
        expr("XNCP_FLOW_CONTROL_TYPE"),
        expr("XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE"),
        expr("XNCP_DEFAULT_RECOMMENDED_TX_POWER_DBM"),
        expr("XNCP_DEFAULT_MAX_TX_POWER_DBM"),
    );

    fs::write(
        PathBuf::from(env::var("OUT_DIR").unwrap()).join("config.rs"),
        generated,
    )
    .unwrap();
}
