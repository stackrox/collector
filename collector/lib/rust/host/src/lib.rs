extern crate cxx;
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
    }

    extern "Rust" {
        type HostInfo;

        fn is_cos(&self) -> bool;
        fn kernel_version(&self) -> Box<KernelVersion>;
        fn os_id(&self) -> String;
        fn build_id(&self) -> String;
        fn distro(&self) -> String;
    }
}
