extern crate cbindgen;

use std::env;
use std::path::Path;

fn main(){
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let target_dir = env::var("BINDINGS_INCLUDE_DIRECTORY").unwrap_or(".".to_string());

    let path = Path::new(&target_dir).join("RustTest.h");

    cbindgen::Builder::new()
        .with_crate(crate_dir)
        .generate()
        .expect("Unable to generate bindings for Test library")
        .write_to_file(path);
}
