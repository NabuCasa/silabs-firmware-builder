use std::env;
use std::fs;
use std::path::PathBuf;

const TILT_THRESHOLD_DEG: f64 = 16.0;
const TILT_HYSTERESIS_DEG: f64 = 4.0;

fn main() {
    let sin2 = |deg: f64| deg.to_radians().sin().powi(2);

    let generated = format!(
        "pub const LED_EFFECTS_TILT_THRESHOLD_SIN2: f32 = {:?};\n\
         pub const LED_EFFECTS_TILT_RELEASE_SIN2: f32 = {:?};\n",
        sin2(TILT_THRESHOLD_DEG) as f32,
        sin2(TILT_THRESHOLD_DEG - TILT_HYSTERESIS_DEG) as f32,
    );

    fs::write(
        PathBuf::from(env::var("OUT_DIR").unwrap()).join("config.rs"),
        generated,
    )
    .unwrap();
}
