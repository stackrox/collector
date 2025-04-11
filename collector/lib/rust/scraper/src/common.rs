pub mod bpf {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

    use std::ffi::CStr;

    impl bpf_prog_result {
        pub fn name(&self) -> String {
            unsafe {
                CStr::from_ptr(self.name.as_ptr())
                    .to_str()
                    .unwrap_or("")
                    .to_string()
            }
        }

        pub fn attached(&self) -> String {
            unsafe {
                CStr::from_ptr(self.attached.as_ptr())
                    .to_str()
                    .unwrap_or("")
                    .to_string()
            }
        }
    }
}
