//! Shared bindgen setup for the components' build scripts.
use bindgen::callbacks::{IntKind, ParseCallbacks};
use std::env;
use std::path::PathBuf;

/// A builder for the crate's `wrapper.h`, with the clang args SLC generated.
pub fn builder() -> bindgen::Builder {
    println!("cargo:rerun-if-env-changed=OHF_BINDGEN_FLAGS");
    println!("cargo:rerun-if-changed=wrapper.h");

    let clang_args: Vec<String> = env::var("OHF_BINDGEN_FLAGS")
        .expect("OHF_BINDGEN_FLAGS (SLC-derived clang args)")
        .split_whitespace()
        .map(str::to_owned)
        .collect();

    bindgen::Builder::default()
        .header("wrapper.h")
        .use_core()
        .layout_tests(false)
        .generate_comments(false)
        // Bare C variant names, e.g. gpioModePushPull
        .prepend_enum_name(false)
        // Constant-fold cast macros such as SL_STATUS_OK
        .clang_macro_fallback()
        .clang_args(&clang_args)
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
