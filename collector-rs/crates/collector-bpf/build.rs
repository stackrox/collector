use std::env;
use std::path::PathBuf;

use libbpf_cargo::SkeletonBuilder;

const BPF_SRC: &str = "src/bpf/collector.bpf.c";

// Multi-arch BPF build notes:
//
// libbpf-cargo reads CARGO_CFG_TARGET_ARCH and automatically passes
// -D__TARGET_ARCH_{arch} to clang, mapping:
//   x86_64   -> __TARGET_ARCH_x86
//   aarch64  -> __TARGET_ARCH_arm64
//   s390x    -> __TARGET_ARCH_s390
//   powerpc64 -> __TARGET_ARCH_powerpc
//
// This selects the correct vmlinux.h via the dispatcher in src/bpf/vmlinux.h
// and configures bpf_tracing.h's PT_REGS_PARM* macros for kprobes.
//
// BPF bytecode is architecture-independent (compiled with -target bpf).
// Cross-compilation works: build on x86_64 for aarch64 by setting
// --target=aarch64-unknown-linux-gnu, and cargo sets CARGO_CFG_TARGET_ARCH
// accordingly.
//
// Endianness: ports use __bpf_ntohs (no-op on big-endian, byte-swap on
// little-endian). IP addresses are stored as raw bytes in network order.

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
    println!("cargo::rerun-if-changed=src/bpf/vmlinux.h");
    println!("cargo::rerun-if-changed=src/bpf/x86_64/vmlinux.h");
    println!("cargo::rerun-if-changed=src/bpf/aarch64/vmlinux.h");
    println!("cargo::rerun-if-changed=src/bpf/s390x/vmlinux.h");
    println!("cargo::rerun-if-changed=src/bpf/ppc64le/vmlinux.h");
}
