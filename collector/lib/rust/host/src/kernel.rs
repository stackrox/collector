use regex::Regex;
use uname::uname;

#[derive(Clone, Default)]
pub struct KernelVersion {
    pub kernel: u64,
    pub major: u64,
    pub minor: u64,
    pub build_id: u64,

    pub release: String,
    pub version: String,
    pub machine: String,
}

impl KernelVersion {
    pub fn new(release: &str, version: &str, machine: &str) -> Self {
        // regex for parsing first parts of release version:
        // ^                   -> must match start of the string
        // (\d+)\.(\d+)\.(\d+) -> match and capture kernel, major, minor versions
        // (-(\d+))?           -> optionally match hyphen followed by build id number
        // .*                  -> matches the rest of the string
        let re = Regex::new(r"^(\d+)\.(\d+)\.(\d+)(-(\d+))?.*").unwrap();

        let Some(captures) = re.captures(release) else {
            return Default::default();
        };

        let kernel = captures[1].parse::<u64>().unwrap_or_default();
        let major = captures[2].parse::<u64>().unwrap_or_default();
        let minor = captures[3].parse::<u64>().unwrap_or_default();

        let build_id = if captures.len() > 5 {
            captures[5].parse::<u64>().unwrap_or_default()
        } else {
            0
        };

        KernelVersion {
            kernel,
            major,
            minor,
            build_id,
            release: release.to_owned(),
            version: version.to_owned(),
            machine: machine.to_owned(),
        }
    }

    pub fn from_host() -> KernelVersion {
        match uname() {
            Ok(uname::Info {
                release,
                version,
                machine,
                ..
            }) => KernelVersion::new(&release, &version, &machine),
            _ => {
                let release = std::env::var("KERNEL_VERSION").unwrap_or_default();
                KernelVersion::new(&release, "", "")
            }
        }
    }

    pub fn has_ebpf_support(&self) -> bool {
        !(self.kernel < 4 || (self.kernel == 4 && self.major < 14))
    }

    pub fn has_secure_boot_param(&self) -> bool {
        !(self.kernel < 4 || (self.kernel == 4 && self.major < 11))
    }

    pub fn short_release(&self) -> String {
        format!("{}.{}.{}", self.kernel, self.major, self.minor)
    }

    pub fn kernel(&self) -> u64 {
        self.kernel
    }

    pub fn major(&self) -> u64 {
        self.major
    }

    pub fn minor(&self) -> u64 {
        self.minor
    }

    pub fn build_id(&self) -> u64 {
        self.build_id
    }

    pub fn release(&self) -> String {
        self.release.clone()
    }

    pub fn version(&self) -> String {
        self.version.clone()
    }

    pub fn machine(&self) -> String {
        self.machine.clone()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_has_ebpf_support() {
        let mut v = KernelVersion {
            kernel: 3,
            major: 13,
            ..Default::default()
        };
        assert!(!v.has_ebpf_support());

        v.kernel = 4;
        assert!(!v.has_ebpf_support());

        v.kernel = 5;
        assert!(v.has_ebpf_support());
    }

    #[test]
    fn test_has_secure_boot_param() {
        let mut v = KernelVersion {
            kernel: 3,
            major: 10,
            ..Default::default()
        };
        assert!(!v.has_secure_boot_param());

        v.kernel = 4;
        assert!(!v.has_secure_boot_param());

        v.kernel = 5;
        assert!(v.has_secure_boot_param());
    }

    #[test]
    fn test_short_release() {
        let v = KernelVersion {
            kernel: 9,
            major: 10,
            minor: 11,
            ..Default::default()
        };

        assert_eq!("9.10.11", v.short_release());
    }

    #[test]
    fn test_kernel_version_construction() {
        let v = KernelVersion::new("3.10.0-957.10.1.el7.x86_64", "", "");
        assert_eq!(3, v.kernel);
        assert_eq!(10, v.major);
        assert_eq!(0, v.minor);
        assert_eq!(957, v.build_id);

        let v = KernelVersion::new("not.a.version-invalid.10.1.el7.x86_64", "", "");
        assert_eq!(0, v.kernel);
        assert_eq!(0, v.major);
        assert_eq!(0, v.minor);
        assert_eq!(0, v.build_id);
    }
}
