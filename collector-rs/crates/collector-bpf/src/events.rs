pub const MAX_FILENAME_LEN: usize = 256;
pub const MAX_ARGS_LEN: usize = 1024;
pub const MAX_CGROUP_LEN: usize = 256;
pub const MAX_COMM_LEN: usize = 16;

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EventType {
    ProcessExec = 1,
    ProcessExit = 2,
    ProcessFork = 3,
    SocketConnect = 10,
    SocketAccept = 11,
    SocketClose = 12,
    SocketListen = 13,
}

impl EventType {
    pub fn from_u32(v: u32) -> Option<Self> {
        match v {
            1 => Some(Self::ProcessExec),
            2 => Some(Self::ProcessExit),
            3 => Some(Self::ProcessFork),
            10 => Some(Self::SocketConnect),
            11 => Some(Self::SocketAccept),
            12 => Some(Self::SocketClose),
            13 => Some(Self::SocketListen),
            _ => None,
        }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EventHeader {
    pub event_type: u32,
    pub pad: u32,
    pub timestamp_ns: u64,
    pub pid: u32,
    pub tid: u32,
    pub uid: u32,
    pub gid: u32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ExecEvent {
    pub header: EventHeader,
    pub ppid: u32,
    pub filename_len: u32,
    pub args_len: u32,
    pub comm_len: u32,
    pub cgroup_len: u32,
    pub _pad: u32,
    pub filename: [u8; MAX_FILENAME_LEN],
    pub args: [u8; MAX_ARGS_LEN],
    pub comm: [u8; MAX_COMM_LEN],
    pub cgroup: [u8; MAX_CGROUP_LEN],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ConnectEvent {
    pub header: EventHeader,
    pub saddr: [u8; 16],
    pub daddr: [u8; 16],
    pub sport: u16,
    pub dport: u16,
    pub family: u16,
    pub protocol: u8,
    pub _pad: u8,
    pub retval: i32,
    pub cgroup_len: u32,
    pub cgroup: [u8; MAX_CGROUP_LEN],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ExitEvent {
    pub header: EventHeader,
    pub exit_code: i32,
    pub _pad: u32,
}

/// Parsed event from the BPF ring buffer.
#[derive(Debug, Clone)]
pub enum RawEvent {
    Exec(ExecEvent),
    Exit(ExitEvent),
    Connect(ConnectEvent),
    Accept(ConnectEvent),
    Close(ConnectEvent),
    Listen(ConnectEvent),
}

impl ExecEvent {
    pub fn filename_str(&self) -> &str {
        let len = (self.filename_len as usize).min(MAX_FILENAME_LEN);
        std::str::from_utf8(&self.filename[..len]).unwrap_or("")
    }

    pub fn comm_str(&self) -> &str {
        let len = (self.comm_len as usize).min(MAX_COMM_LEN);
        std::str::from_utf8(&self.comm[..len]).unwrap_or("")
    }

    pub fn cgroup_str(&self) -> &str {
        let len = (self.cgroup_len as usize).min(MAX_CGROUP_LEN);
        std::str::from_utf8(&self.cgroup[..len]).unwrap_or("")
    }

    pub fn args_bytes(&self) -> &[u8] {
        let len = (self.args_len as usize).min(MAX_ARGS_LEN);
        &self.args[..len]
    }
}

impl ConnectEvent {
    pub fn cgroup_str(&self) -> &str {
        let len = (self.cgroup_len as usize).min(MAX_CGROUP_LEN);
        std::str::from_utf8(&self.cgroup[..len]).unwrap_or("")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem;

    #[test]
    fn event_type_roundtrip() {
        assert_eq!(EventType::from_u32(1), Some(EventType::ProcessExec));
        assert_eq!(EventType::from_u32(10), Some(EventType::SocketConnect));
        assert_eq!(EventType::from_u32(99), None);
    }

    #[test]
    fn struct_sizes_are_stable() {
        // These sizes must match the C-side definitions
        assert_eq!(mem::size_of::<EventHeader>(), 32);
        assert_eq!(mem::size_of::<ExitEvent>(), 40);
    }

    #[test]
    fn exec_event_string_extraction() {
        let mut evt = unsafe { mem::zeroed::<ExecEvent>() };
        let name = b"/usr/bin/ls";
        evt.filename[..name.len()].copy_from_slice(name);
        evt.filename_len = name.len() as u32;
        assert_eq!(evt.filename_str(), "/usr/bin/ls");
    }

    #[test]
    fn exec_event_truncated_len() {
        let mut evt = unsafe { mem::zeroed::<ExecEvent>() };
        evt.filename_len = 9999; // exceeds buffer
        // Should not panic, clamps to buffer size
        let _ = evt.filename_str();
    }
}
