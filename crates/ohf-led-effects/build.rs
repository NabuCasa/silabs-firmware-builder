use std::env;
use std::fs;
use std::path::PathBuf;

// Defaults match the component's config header
fn main() {
    println!("cargo:rerun-if-env-changed=OHF_RUST_CONFIG");

    let path = env::var("OHF_RUST_CONFIG").expect("OHF_RUST_CONFIG (resolved manifest config)");
    println!("cargo:rerun-if-changed={path}");
    let cfg: serde_json::Value = serde_json::from_str(&fs::read_to_string(&path).unwrap()).unwrap();

    let get = |key: &str, default: &str| {
        cfg.get(key)
            .and_then(|v| v.as_str())
            .unwrap_or(default)
            .to_owned()
    };

    let threshold: f64 = get("LED_EFFECTS_TILT_THRESHOLD_DEG", "16").parse().unwrap();
    let hysteresis: f64 = get("LED_EFFECTS_TILT_HYSTERESIS_DEG", "4").parse().unwrap();
    assert!(0.0 <= threshold - hysteresis && threshold <= 90.0);
    let sin2 = |deg: f64| deg.to_radians().sin().powi(2);

    let generated = format!(
        "pub const LED_EFFECTS_UPDATE_INTERVAL_MS: u32 = {} as u32;\n\
         pub const LED_EFFECTS_TILT_THRESHOLD_SIN2: f32 = {:?};\n\
         pub const LED_EFFECTS_TILT_RELEASE_SIN2: f32 = {:?};\n",
        get("LED_EFFECTS_UPDATE_INTERVAL_MS", "4"),
        sin2(threshold) as f32,
        sin2(threshold - hysteresis) as f32,
    );

    fs::write(
        PathBuf::from(env::var("OUT_DIR").unwrap()).join("config.rs"),
        generated,
    )
    .unwrap();
}
