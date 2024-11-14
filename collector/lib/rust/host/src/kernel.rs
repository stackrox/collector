use regex::Regex;
use uname::uname;

pub struct KernelVersion {
    kernel: u64,
    major: u64,
    minor: u64,
    build_id: u64,

    release: String,
    version: String,
    machine: String,
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

        match re.captures(release) {
            Some(captures) => KernelVersion {
                kernel: captures[1].parse::<u64>().unwrap(),
                major: captures[2].parse::<u64>().unwrap(),
                minor: captures[3].parse::<u64>().unwrap(),
                build_id: if captures.len() > 4 {
                    captures[4].parse::<u64>().unwrap()
                } else {
                    0
                },
                release: release.to_owned(),
                version: version.to_owned(),
                machine: machine.to_owned(),
            },
            None => Default::default(),
        }
    }

    pub fn from_host() -> Box<KernelVersion> {
        let kv = match uname() {
            Ok(info) => KernelVersion::new(&info.release, &info.version, &info.machine),
            _ => {
                let release = std::env::var("KERNEL_VERSION").unwrap_or_default();
                KernelVersion::new(&release, &"", &"")
            }
        };

        Box::new(kv)
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
