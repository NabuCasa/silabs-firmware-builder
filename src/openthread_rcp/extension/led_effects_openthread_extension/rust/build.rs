fn main() {
    ohf_bindgen::write(
        ohf_bindgen::builder()
            .allowlist_function("otLinkGetPanId")
            .allowlist_function("otPlatRadioIsEnabled"),
    );
}
