use std::env;
use std::fs;
use std::path::PathBuf;

// Generate the component's config consts from the resolved manifest values that
// build_project.py forwards (rust_build.json). The manifest is the single source of
// truth; nothing is hardcoded here.
fn main() {
    println!("cargo:rerun-if-env-changed=OHF_RUST_CONFIG");

    let path = env::var("OHF_RUST_CONFIG").expect("OHF_RUST_CONFIG (resolved manifest config)");
    println!("cargo:rerun-if-changed={path}");
    let cfg: serde_json::Value = serde_json::from_str(&fs::read_to_string(&path).unwrap()).unwrap();

    let value = |key: &str| cfg[key].as_str().unwrap_or_else(|| panic!("{key} missing from config")).to_owned();

    // A value is either a numeric literal or an SDK symbol (e.g. SL_GPIO_PORT_C) that
    // bindgen emits in ohf-sys. Emit it verbatim as a Rust expression either way.
    let expr = |v: &str| {
        if v.as_bytes()[0].is_ascii_digit() {
            v.to_owned()
        } else {
            format!("ohf_sys::{v}")
        }
    };

    let generated = format!(
        "pub const WS2812_NUM_LEDS: usize = {} as usize;\n\
         pub const WS2812_EN_PIN: u32 = {} as u32;\n\
         pub const WS2812_EN_PORT: u32 = {} as u32;\n",
        expr(&value("WS2812_NUM_LEDS")),
        expr(&value("WS2812_EN_PIN")),
        expr(&value("WS2812_EN_PORT")),
    );

    let dest = PathBuf::from(env::var("OUT_DIR").unwrap()).join("config.rs");
    fs::write(dest, generated).unwrap();
}
