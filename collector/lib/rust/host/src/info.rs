use crate::KernelVersion;

use std::fs::File;
use std::io::{prelude::*, BufReader};
use std::path::PathBuf;

// The values are taken from efi_secureboot_mode in include/linux/efi.h
#[derive(Default)]
pub enum SecureBootStatus {
    ENABLED = 3,
    DISABLED = 2,
    // Secure Boot seems to be disabled, but the boot loaded
    // does not provide enough information about it,
    // so it could be enabled without the kernel being aware.
    #[default]
    NOT_DETERMINED = 1,
    UNSET = 0, // No detection is performed yet
}

pub struct HostInfo;

impl HostInfo {
    pub fn is_cos(&self) -> bool {
        self.os_id().eq("cos") && !self.build_id().is_empty()
    }

    pub fn is_coreos(&self) -> bool {
        self.os_id().eq("coreos")
    }

    pub fn is_docker_desktop(&self) -> bool {
        self.os_id().eq("Docker Desktop")
    }

    pub fn is_ubuntu(&self) -> bool {
        self.os_id().eq("ubuntu")
    }

    pub fn is_garden(&self) -> bool {
        self.distro().contains("Garden Linux")
    }

    pub fn kernel_version(&self) -> Box<KernelVersion> {
        KernelVersion::from_host()
    }

    pub fn os_id(&self) -> String {
        self.os_release_key("ID")
    }

    pub fn build_id(&self) -> String {
        self.os_release_key("BUILD_ID")
    }

    pub fn distro(&self) -> String {
        self.os_release_key("PRETTY_NAME")
    }

    fn os_release_key(&self, name: &str) -> String {
        self.filter_by_key("/etc/os-release", name)
    }

    fn filter_by_key<T: Into<PathBuf>>(&self, path: T, name: &str) -> String {
        if let Ok(file) = File::open(path.into()) {
            let reader = BufReader::new(file);

            for line in reader.lines() {
                let Ok(line) = line else {
                    break;
                };

                if line.starts_with(name) {
                    return line[name.len()..].to_owned();
                }
            }
        }

        Default::default()
    }
}
