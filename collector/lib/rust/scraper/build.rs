use libbpf_cargo::SkeletonBuilder;
use std::io;
use std::path::PathBuf;
use std::{env, fs};

fn make_skeleton(src: PathBuf) {
    let src_file = src.file_name().unwrap();

    let mut dst_path: PathBuf = PathBuf::from(src_file);
    dst_path.set_extension("skel.rs");

    let base = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR must be set in build script"));

    let out = base.join(&dst_path);

    let mut obj_path: PathBuf = out.clone();
    obj_path.set_extension("o");
    obj_path = base.join(&obj_path);

    SkeletonBuilder::new()
        .source(&src)
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
                make_skeleton(path.clone());
                println!("cargo:rerun-if-changed={}", path.display());
            }
        }
    }

    Ok(())
}
