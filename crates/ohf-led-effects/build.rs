use std::env;
use std::fs;
use std::path::PathBuf;

// Config consts from the resolved manifest (rust_build.json). These have defaults in
// the SDK config header; the Rust crate owns the same defaults, overridden if the
// manifest sets them.
fn main() {
    println!("cargo:rerun-if-env-changed=OHF_RUST_CONFIG");

    let path = env::var("OHF_RUST_CONFIG").expect("OHF_RUST_CONFIG (resolved manifest config)");
    println!("cargo:rerun-if-changed={path}");
    let cfg: serde_json::Value = serde_json::from_str(&fs::read_to_string(&path).unwrap()).unwrap();

    let get = |key: &str, default: &str| {
        cfg.get(key).and_then(|v| v.as_str()).unwrap_or(default).to_owned()
    };

    let generated = format!(
        "pub const LED_EFFECTS_UPDATE_INTERVAL_MS: u32 = {} as u32;\n\
         pub const LED_EFFECTS_TILT_THRESHOLD_DEG: f32 = {} as f32;\n\
         pub const LED_EFFECTS_TILT_HYSTERESIS_DEG: f32 = {} as f32;\n",
        get("LED_EFFECTS_UPDATE_INTERVAL_MS", "4"),
        get("LED_EFFECTS_TILT_THRESHOLD_DEG", "16"),
        get("LED_EFFECTS_TILT_HYSTERESIS_DEG", "4"),
    );

    fs::write(PathBuf::from(env::var("OUT_DIR").unwrap()).join("config.rs"), generated).unwrap();
}
