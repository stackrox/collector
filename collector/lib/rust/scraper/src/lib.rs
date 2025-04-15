pub mod bpf;
pub mod common;
pub mod network;

#[cxx::bridge(namespace = "collector::rust")]
mod ffi {
    extern "Rust" {
        fn get_bpf_programs() -> Vec<::common::bpf::bpf_prog_result>;
    }
}

pub fn get_bpf_programs() -> Vec<common::bpf::bpf_prog_result> {
    vec![]
}
