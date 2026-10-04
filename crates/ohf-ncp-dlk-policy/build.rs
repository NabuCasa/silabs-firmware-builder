// Clang flags come from SLC's generated project via OHF_BINDGEN_FLAGS. The shared bindgen
// setup lives in ohf-bindgen; only this crate's allowlist is stated here — one entry per
// symbol (or SL_ENUM `.*` group), which bindgen accumulates.
fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            // DLK negotiation enums: SL_ENUM, so the variants live in the `.*_enum` types.
            .allowlist_type("sl_zigbee_dlk_.*")
            .allowlist_type("sl_zigbee_address_info")
            .allowlist_type("sl_zigbee_sec_man_.*")
            .allowlist_type("sl_status_t")
            .allowlist_function("sl_zigbee_sec_man_init_context")
            .allowlist_function("sl_zigbee_sec_man_get_aps_key_info")
            .allowlist_var("DLK_PROTOCOL_.*")
            .allowlist_var("DLK_SECRET_.*")
            .allowlist_var("SL_ZB_SEC_MAN_KEY_TYPE_TC_LINK_WITH_TIMEOUT")
            .allowlist_var("ZB_SEC_MAN_FLAG_EUI_IS_VALID")
            .allowlist_var("EUI64_SIZE")
            .allowlist_var("SL_STATUS_OK")
            .allowlist_var("SL_STATUS_NOT_SUPPORTED")
            .allowlist_var("SL_STATUS_NOT_FOUND"),
    );
}
