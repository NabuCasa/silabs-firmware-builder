use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    let cfg = ohf_config::Config::load("ws2812");

    // Symbolic values (e.g. SL_GPIO_PORT_C) come from the ohf-sys bindings
    let expr = |key: &str| {
        let v = cfg.get(key);
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
        expr("WS2812_NUM_LEDS"),
        expr("WS2812_EN_PIN"),
        expr("WS2812_EN_PORT"),
    );

    let dest = PathBuf::from(env::var("OUT_DIR").unwrap()).join("config.rs");
    fs::write(dest, generated).unwrap();
}
