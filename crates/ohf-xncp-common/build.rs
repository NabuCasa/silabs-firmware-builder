// Clang flags come from SLC's generated project via OHF_BINDGEN_FLAGS. The shared bindgen
// setup lives in ohf-bindgen; only this crate's allowlist is stated here — one entry per
// symbol (or `.*` group), which bindgen accumulates.
fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("sl_zigbee_aps_frame_t")
            .allowlist_type("sli_zigbee_route_table_entry_t")
            .allowlist_type("sl_status_t")
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
            .allowlist_var("SL_ZIGBEE_MAX_SOURCE_ROUTE_RELAY_COUNT")
            .allowlist_var("SL_ZIGBEE_EZSP_MFG_.*")
            .allowlist_var("RAM_MEM_SIZE")
            .allowlist_var("PART_NUMBER")
            // The resolved XNCP_* config macros from the generated xncp_config.h.
            .allowlist_var("XNCP_.*")
            .allowlist_var("usartHwFlowControl.*"),
    );
}
