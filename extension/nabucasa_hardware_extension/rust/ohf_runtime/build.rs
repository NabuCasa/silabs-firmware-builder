fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_type("CORE_irqState_t")
            .allowlist_function("CORE_EnterCritical")
            .allowlist_function("CORE_ExitCritical"),
    );
}
