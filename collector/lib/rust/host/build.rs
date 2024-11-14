extern crate cxx_build;

use cxx_build::CFG;

fn main() {
    CFG.doxygen = true;

    let _build = cxx_build::bridge("src/lib.rs");
}
