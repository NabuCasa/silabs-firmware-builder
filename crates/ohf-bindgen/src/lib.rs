//! Shared bindgen scaffolding for the OHF component crates (a build-dependency).
//!
//! Every crate that binds SDK headers needs the same setup: the clang args SLC emitted
//! (`OHF_BINDGEN_FLAGS`, forwarded by build_project.py), `no_std` output, bare enum-variant
//! names, the macro-constant fallback, and writing `OUT_DIR/bindings.rs`. Only the allowlist
//! — which symbols the crate actually binds — differs, so that's all a component's build.rs
//! supplies: `ohf_bindgen::write(ohf_bindgen::builder().allowlist_type(..)...)`.
use std::env;
use std::path::PathBuf;

/// A `bindgen::Builder` with the common OHF options and the SLC-derived clang args applied.
/// The caller adds its `allowlist_*` (and any crate-specific tweak like `opaque_type`) and
/// hands the result to [`write`]. Emits the rerun-if lines for a crate-root `wrapper.h`.
///
/// Missing/garbage flags should crash the build, not silently degrade it.
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
        // Emit enum variants under their bare C names (gpioModePushPull, DLK_PROTOCOL_*),
        // not EnumName_variant. SL_ENUM variants live in a separate `name_enum` type, which
        // the caller allowlists by name.
        .prepend_enum_name(false)
        // Cast macros (SL_STATUS_OK, config symbols) bindgen can't constant-fold alone.
        .clang_macro_fallback()
        .clang_args(&clang_args)
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
