fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("SPIDRV_Handle_t")
            .allowlist_function("SPIDRV_MTransmit")
            // The project's SPI instance
            .allowlist_var("sl_spidrv_eusart_ws2812_handle")
            .opaque_type("SPIDRV_HandleData"),
    );

    // Symbolic values (e.g. SL_GPIO_PORT_C) come from the ohf-sys bindings
    ohf_config::generate("ohf_sys");
}
