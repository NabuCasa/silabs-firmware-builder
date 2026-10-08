//! The `config` a component's slcc declares, resolved against the manifest.
use std::cell::RefCell;
use std::collections::{BTreeMap, BTreeSet};
use std::env;
use std::fs;

use cexpr::expr::EvalResult;

/// Every declared key must be read, so a value the manifest sets always takes effect.
pub struct Config {
    values: BTreeMap<String, String>,
    read: RefCell<BTreeSet<String>>,
}

impl Config {
    /// The config of the calling crate, by its package name
    pub fn load() -> Self {
        let feature = env::var("CARGO_PKG_NAME").unwrap();
        println!("cargo:rerun-if-env-changed=OHF_RUST_CONFIG");
        let path = env::var("OHF_RUST_CONFIG").unwrap();
        println!("cargo:rerun-if-changed={path}");

        let mut all: BTreeMap<String, BTreeMap<String, String>> =
            serde_json::from_str(&fs::read_to_string(&path).unwrap()).unwrap();

        Self {
            values: all
                .remove(&feature)
                .unwrap_or_else(|| panic!("{feature} has no config")),
            read: RefCell::new(BTreeSet::new()),
        }
    }

    pub fn get(&self, key: &str) -> &str {
        self.read.borrow_mut().insert(key.to_owned());
        self.values
            .get(key)
            .unwrap_or_else(|| panic!("{key} is not in rust_config"))
    }

    /// A string's bytes. A value in double quotes is a C string literal, as a C define
    /// would need, and its escapes are decoded.
    pub fn get_bytes(&self, key: &str) -> Vec<u8> {
        let v = self.get(key);
        if !v.starts_with('"') {
            return v.as_bytes().to_vec();
        }

        match cexpr::literal::parse(v.as_bytes()).unwrap().1 {
            EvalResult::Str(s) => s,
            other => panic!("{key} is not a C string literal: {other:?}"),
        }
    }
}

impl Drop for Config {
    fn drop(&mut self) {
        let read = self.read.borrow();
        let unread: Vec<_> = self.values.keys().filter(|k| !read.contains(*k)).collect();
        assert!(unread.is_empty(), "config declares unread keys: {unread:?}");
    }
}
