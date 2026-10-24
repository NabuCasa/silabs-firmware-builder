//! The `config` a component's slcc declares, resolved against the manifest by the build
//! tool, emitted as the crate's constants.
use std::collections::BTreeMap;
use std::env;
use std::fs;
use std::path::PathBuf;

use cexpr::expr::EvalResult;

/// Write the calling crate's constants to `OUT_DIR/config.rs`. A symbolic value, such as
/// an SDK enum variant, resolves in the module at `symbols`.
pub fn generate(symbols: &str) {
    println!("cargo:rerun-if-env-changed=OHF_RUST_CONFIG");
    let path = env::var("OHF_RUST_CONFIG").unwrap();
    println!("cargo:rerun-if-changed={path}");

    let crate_name = env::var("CARGO_PKG_NAME").unwrap();
    let mut all: BTreeMap<String, BTreeMap<String, BTreeMap<String, String>>> =
        serde_json::from_str(&fs::read_to_string(&path).unwrap()).unwrap();
    let config = all
        .remove(&crate_name)
        .unwrap_or_else(|| panic!("{crate_name} has no config"));

    let mut generated = String::new();
    for (name, entry) in &config {
        let ty = entry["type"].as_str();
        let value = entry["value"].as_str();
        let (rust_type, literal) = match ty {
            "bytes" => ("&[u8]".to_owned(), byte_string(value)),
            _ if value.starts_with(|c: char| c.is_ascii_digit() || c == '-') => {
                (ty.to_owned(), format!("{value} as {ty}"))
            }
            _ => (ty.to_owned(), format!("{symbols}::{value} as {ty}")),
        };
        generated += &format!("pub(crate) const {name}: {rust_type} = {literal};\n");
    }

    fs::write(
        PathBuf::from(env::var("OUT_DIR").unwrap()).join("config.rs"),
        generated,
    )
    .unwrap();
}

/// A `b"..."` literal. A value in double quotes is a C string literal, as a C define
/// would need, and its escapes are decoded.
fn byte_string(value: &str) -> String {
    let bytes = if value.starts_with('"') {
        match cexpr::literal::parse(value.as_bytes()).unwrap().1 {
            EvalResult::Str(s) => s,
            other => panic!("{value} is not a C string literal: {other:?}"),
        }
    } else {
        value.as_bytes().to_vec()
    };
    let escaped: String = bytes
        .into_iter()
        .flat_map(std::ascii::escape_default)
        .map(char::from)
        .collect();
    format!("b\"{escaped}\"")
}
