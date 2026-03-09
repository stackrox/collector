use std::env;
use std::path::PathBuf;

use libbpf_cargo::SkeletonBuilder;

const BPF_SRC: &str = "src/bpf/collector.bpf.c";

fn main() {
    let out_dir = PathBuf::from(
        env::var_os("OUT_DIR").expect("OUT_DIR not set"),
    );

    let skel_path = out_dir.join("collector.skel.rs");

    SkeletonBuilder::new()
        .source(BPF_SRC)
        .build_and_generate(&skel_path)
        .expect("Failed to build BPF skeleton");

    println!("cargo::rerun-if-changed={BPF_SRC}");
}
