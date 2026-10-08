//! Shared bindgen setup for the components' build scripts.
use bindgen::callbacks::{IntKind, ParseCallbacks};
use std::env;
use std::path::PathBuf;

/// A builder for the crate's `wrapper.h`, with the clang args SLC generated.
pub fn builder() -> bindgen::Builder {
    println!("cargo:rerun-if-env-changed=OHF_BINDGEN_FLAGS");

    let clang_args: Vec<String> = env::var("OHF_BINDGEN_FLAGS")
        .expect("OHF_BINDGEN_FLAGS (SLC-derived clang args)")
        .split_whitespace()
        .map(str::to_owned)
        .collect();

    // The crate's cargo features, for `#ifdef CARGO_FEATURE_*` in wrapper.h
    let features = env::vars().filter_map(|(name, _)| {
        name.starts_with("CARGO_FEATURE_")
            .then(|| format!("-D{name}"))
    });

    bindgen::Builder::default()
        .header("wrapper.h")
        .use_core()
        // Every crate reports statuses
        .allowlist_type("sl_status_t")
        .allowlist_var("SL_STATUS_.*")
        .layout_tests(false)
        .generate_comments(false)
        // Bare C variant names, e.g. gpioModePushPull
        .prepend_enum_name(false)
        // Constant-fold cast macros such as SL_STATUS_OK
        .clang_macro_fallback()
        // `rerun-if-changed` for wrapper.h and every header it includes, so a shared
        // target dir rebuilds the bindings when the generated project changes
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .clang_args(&clang_args)
        .clang_args(features)
}

pub trait BuilderExt {
    /// `allowlist_var` for an integer macro, typed as `ty` instead of bindgen's default `u32`.
    fn allowlist_var_as(self, name: &'static str, ty: &'static str) -> Self;
}

impl BuilderExt for bindgen::Builder {
    fn allowlist_var_as(self, name: &'static str, ty: &'static str) -> Self {
        self.allowlist_var(name)
            .parse_callbacks(Box::new(MacroType { name, ty }))
    }
}

#[derive(Debug)]
struct MacroType {
    name: &'static str,
    ty: &'static str,
}

impl ParseCallbacks for MacroType {
    fn int_macro(&self, name: &str, _value: i64) -> Option<IntKind> {
        (name == self.name).then_some(IntKind::Custom {
            name: self.ty,
            is_signed: self.ty.starts_with('i'),
        })
    }
}

/// Generate the bindings and write them to `OUT_DIR/bindings.rs`.
pub fn write(builder: bindgen::Builder) {
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    builder
        .generate()
        .expect("bindgen failed")
        .write_to_file(out.join("bindings.rs"))
        .unwrap();
}
