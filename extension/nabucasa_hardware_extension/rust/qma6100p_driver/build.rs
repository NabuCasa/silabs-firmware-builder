fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("I2C_TransferSeq_TypeDef")
            .allowlist_function("I2CSPM_Transfer")
            .allowlist_function("sl_udelay_wait")
            .allowlist_var("I2C_FLAG_.*")
            // The project's I2C instance
            .allowlist_var("sl_i2cspm_inst"),
    );
}
