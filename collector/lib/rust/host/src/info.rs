use crate::KernelVersion;

use std::fs::File;
use std::io::{prelude::*, BufReader};
use std::path::PathBuf;

const MIN_RHEL_BUILD_ID: u64 = 957;

// The values are taken from efi_secureboot_mode in include/linux/efi.h
#[derive(Default)]
pub enum SecureBootStatus {
    #[allow(dead_code)]
    Enabled = 3,
    #[allow(dead_code)]
    Disabled = 2,

    // Secure Boot seems to be disabled, but the boot loaded
    // does not provide enough information about it,
    // so it could be enabled without the kernel being aware.
    #[allow(dead_code)]
    NotDetermined = 1,

    // No detection is performed yet
    #[default]
    Unset = 0,
}

pub struct HostInfo {
    os: String,
    build: String,
    distro: String,
    hostname: String,
    kernel: KernelVersion,
}

impl HostInfo {
    pub fn new() -> Self {
        let mut os = String::new();
        let mut build = String::new();
        let mut distro = String::new();
        let mut hostname = String::new();

        if let Ok(file) = File::open("/etc/os-release") {
            let reader = BufReader::new(file);

            for line in reader.lines() {
                match line {
                    Ok(s) if s.starts_with("ID") => os = s["ID ".len()..].to_owned(),
                    Ok(s) if s.starts_with("BUILD_ID") => build = s["BUILD_ID ".len()..].to_owned(),
                    Ok(s) if s.starts_with("PRETTY_NAME") => {
                        distro = s["PRETTY_NAME ".len()..].to_owned()
                    }
                    Ok(_) => {}
                    Err(_) => break,
                };
            }
        }

        let hostname_paths = vec![
            host_path("/etc/hostname"),
            host_path("/proc/sys/kernel/hostname"),
        ];
        for path in hostname_paths {
            if let Ok(mut file) = File::open(path) {
                if file.read_to_string(&mut hostname).is_ok() {
                    break;
                }
            }
        }

        HostInfo {
            os,
            build,
            distro,
            hostname,
            kernel: KernelVersion::from_host(),
        }
    }

    /// Whether or not this host machine is COS
    pub fn is_cos(&self) -> bool {
        self.os_id().eq("cos") && !self.build_id().is_empty()
    }

    /// Whether or not this host machine is CORE-OS
    pub fn is_coreos(&self) -> bool {
        self.os_id().eq("coreos")
    }

    /// Whether or not this host machine is DockerDesktop
    pub fn is_docker_desktop(&self) -> bool {
        self.os_id().eq("Docker Desktop")
    }

    /// Whether or not this host machine is Ubuntu
    pub fn is_ubuntu(&self) -> bool {
        self.os_id().eq("ubuntu")
    }

    /// Whether or not this host machine is Garden Linux
    pub fn is_garden(&self) -> bool {
        self.distro().contains("Garden Linux")
    }

    /// Gets the KernelVersion for this Host
    /// This must return a Box in order to maintain
    /// FFI compatibility
    pub fn kernel_version(&self) -> Box<KernelVersion> {
        Box::new(self.kernel.clone())
    }

    /// Gets the ID of this host, populated from /etc/os-release
    pub fn os_id(&self) -> String {
        self.os.clone()
    }

    /// Gets the Build ID of this host, populated from /etc/os-release
    pub fn build_id(&self) -> String {
        self.build.clone()
    }

    /// Gets the distribution of this host, populated from /etc/os-release
    pub fn distro(&self) -> String {
        self.distro.clone()
    }

    /// Gets the hostname of this host, populated either from the
    /// environment, or from a best-guess look at the filesystem
    /// (/etc/hostname or /proc/sys/kernel/hostname)
    pub fn hostname(&self) -> String {
        self.hostname.clone()
    }

    pub fn is_rhel76(&self) -> bool {
        let k = &self.kernel;
        if self.os_id() == "rhel" || self.os_id() == "centos" && k.release.contains(".el7.") {
            return (k.kernel == 3 && k.major == 10) && k.build_id >= MIN_RHEL_BUILD_ID;
        }
        false
    }

    pub fn is_rhel86(&self) -> bool {
        self.os_id() == "rhel" || self.os_id() == "rhcos" && self.kernel.release.contains(".el8_6")
    }

    pub fn has_ebpf_support(&self) -> bool {
        self.is_rhel76() || self.kernel.has_ebpf_support()
    }

    pub fn has_btf_symbols(&self) -> bool {
        // TODO: not sure I like the layout of this, perhaps it needs a revisit
        let locations = vec![
            // try canonical vmlinux BTF through sysfs first
            ("/sys/kernel/btf/vmlinux".to_string(), false),
            // fall back to trying to find vmlinux on disk otherwise
            (format!("/boot/vmlinux-{}", self.kernel.release), false),
            (
                format!(
                    "/lib/modules/{}/vmlinux-{}",
                    self.kernel.release, self.kernel.release
                ),
                false,
            ),
            (
                format!("/lib/modules/{}/build/vmlinux", self.kernel.release),
                false,
            ),
            (
                format!("/usr/lib/modules/{}/kernel/vmlinux", self.kernel.release),
                true,
            ),
            (
                format!("/usr/lib/debug/boot/vmlinux-{}", self.kernel.release),
                true,
            ),
            (
                format!("/usr/lib/debug/boot/vmlinux-{}.debug", self.kernel.release),
                true,
            ),
            (
                format!("/usr/lib/debug/lib/modules/{}/vmlinux", self.kernel.release),
                true,
            ),
        ];

        for location in locations {
            let path = if location.1 {
                host_path(location.0)
            } else {
                PathBuf::from(location.0)
            };

            if path.exists() {
                return true;
            }
        }

        false
    }

    pub fn is_uefi(&self) -> bool {
        match std::fs::metadata(host_path("/sys/firmware/efi")) {
            Ok(stat) => stat.is_dir(),
            // TODO: do some logging with the error
            Err(_) => false,
        }
    }

    #[allow(dead_code)]
    pub fn secure_boot_status(&self) -> SecureBootStatus {
        SecureBootStatus::Unset
    }
}

/// Constructs a path joined from the host root (usually /host)
fn host_path<P: Into<PathBuf>>(path: P) -> PathBuf {
    let root = PathBuf::from(std::env::var("COLLECTOR_HOST_PATH").unwrap_or("/host".to_string()));
    root.join(path.into())
}

/// FFI-compatible constructor for HostInfo objects
pub fn host_info() -> Box<HostInfo> {
    Box::new(HostInfo::new())
}
