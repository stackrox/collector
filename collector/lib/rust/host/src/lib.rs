extern crate cxx;
extern crate libbpf_rs;
extern crate libc;
extern crate log;
extern crate regex;
extern crate uname;

mod info;
mod kernel;

use crate::info::*;
use crate::kernel::*;

#[cxx::bridge(namespace = "collector::rust")]
mod ffi {
    extern "Rust" {
        type KernelVersion;
        fn has_ebpf_support(&self) -> bool;
        fn has_secure_boot_param(&self) -> bool;
        fn short_release(&self) -> String;
        fn kernel(&self) -> u64;
        fn major(&self) -> u64;
        fn minor(&self) -> u64;
        fn build_id(&self) -> u64;
        fn release(&self) -> String;
        fn version(&self) -> String;
        fn machine(&self) -> String;
    }

    extern "Rust" {
        type HostInfo;

        fn is_cos(&self) -> bool;
        fn kernel_version(&self) -> Box<KernelVersion>;
        fn os_id(&self) -> String;
        fn build_id(&self) -> String;
        fn distro(&self) -> String;
        fn hostname(&self) -> String;
        fn is_coreos(&self) -> bool;
        fn is_docker_desktop(&self) -> bool;
        fn is_ubuntu(&self) -> bool;
        fn is_garden(&self) -> bool;
        fn is_rhel76(&self) -> bool;
        fn is_rhel86(&self) -> bool;
        fn has_ebpf_support(&self) -> bool;
        fn has_btf_symbols(&self) -> bool;
        fn is_uefi(&self) -> bool;
        fn num_possible_cpu(&self) -> i64;
        fn has_bpf_tracing_support(&self) -> bool;
        fn has_bpf_ringbuf_support(&self) -> bool;
    }

    extern "Rust" {
        fn host_info() -> Box<HostInfo>;
    }
}
