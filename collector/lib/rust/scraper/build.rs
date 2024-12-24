use libbpf_cargo::SkeletonBuilder;
use std::io;
use std::path::PathBuf;
use std::{env, fs};

fn make_skeleton(src: PathBuf) {
    let src_path = src.clone();

    let mut dst_path: PathBuf = src.clone();
    dst_path.set_extension("skel.rs");

    let base = PathBuf::from(
        env::var_os("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR must be set in build script"),
    );

    let out = base.join("src").join("bpf").join(&dst_path);

    let mut obj_path: PathBuf = src.clone();
    obj_path.set_extension("o");
    obj_path = base.join("src").join("bpf").join(&obj_path);

    SkeletonBuilder::new()
        .source(&src_path)
        .clang_args([
            "-I",
            vmlinux::include_path_root()
                .join("x86_64")
                .to_str()
                .unwrap(),
        ])
        .obj(&obj_path)
        .build_and_generate(&out)
        .unwrap();

    let display = src_path.display();
    println!("cargo:rerun-if-changed={display}");
}

fn main() -> io::Result<()> {
    let bpf_dir = PathBuf::from(
        env::var_os("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR must be set in build script"),
    )
    .join("src")
    .join("bpf");

    for file in fs::read_dir(bpf_dir)? {
        let entry = file?;
        let path = entry.path();

        if let Some(ext) = path.extension() {
            if ext == "c" {
                make_skeleton(path);
            }
        }
    }

    Ok(())
}
