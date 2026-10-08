use ohf_bindgen::BuilderExt;

fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("sl_zigbee_aps_frame_t")
            .allowlist_type("sli_zigbee_route_table_entry_t")
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

    // Symbolic values (e.g. SL_IOSTREAM_EUSART_UART_FLOW_CTRL_NONE) come from the bindings
    ohf_config::generate("bindings");
}
