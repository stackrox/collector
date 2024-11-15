use regex::Regex;
use uname::uname;

#[derive(Clone)]
pub struct KernelVersion {
    pub kernel: u64,
    pub major: u64,
    pub minor: u64,
    pub build_id: u64,

    pub release: String,
    pub version: String,
    pub machine: String,
}

impl Default for KernelVersion {
    fn default() -> Self {
        KernelVersion {
            kernel: 0,
            major: 0,
            minor: 0,
            build_id: 0,
            release: "".to_string(),
            version: "".to_string(),
            machine: "".to_string(),
        }
    }
}

impl KernelVersion {
    pub fn new(release: &str, version: &str, machine: &str) -> Self {
        // regex for parsing first parts of release version:
        // ^                   -> must match start of the string
        // (\d+)\.(\d+)\.(\d+) -> match and capture kernel, major, minor versions
        // (-(\d+))?           -> optionally match hyphen followed by build id number
        // .*                  -> matches the rest of the string
        let re = Regex::new(r"(^(\d+)\.(\d+)\.(\d+)(-(\d+))?.*)").unwrap();

        let Some(captures) = re.captures(release) else {
            return Default::default();
        };

        let kernel = captures[1].parse::<u64>().unwrap();
        let major = captures[2].parse::<u64>().unwrap();
        let minor = captures[3].parse::<u64>().unwrap();

        let build_id = if captures.len() > 4 {
            captures[4].parse::<u64>().unwrap()
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
                KernelVersion::new(&release, &"", &"")
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
}
