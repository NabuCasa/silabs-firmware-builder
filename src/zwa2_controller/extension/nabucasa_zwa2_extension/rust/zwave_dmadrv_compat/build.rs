fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("zpal_uart_transmit_done_t")
            .allowlist_type("DMADRV_DataSize_t")
            .allowlist_type("LDMA_CtrlSize_t")
            .allowlist_function("DMADRV_Init")
            .allowlist_function("DMADRV_AllocateChannelById")
            .allowlist_function("DMADRV_MemoryPeripheral")
            .allowlist_var("ECODE_EMDRV_DMADRV_OK")
            .allowlist_var("ECODE_EMDRV_DMADRV_ALREADY_INITIALIZED"),
    );

    ohf_config::generate("bindings");
}
