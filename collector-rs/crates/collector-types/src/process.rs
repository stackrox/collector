use crate::container::ContainerId;

/// Per-process metadata captured from exec events and stored in the process table.
#[derive(Debug, Clone)]
pub struct ProcessInfo {
    pub pid: u32,
    pub uid: u32,
    pub gid: u32,
    pub exe_path: String,
    pub args: String,
    pub container_id: ContainerId,
}

/// A single ancestor in a process's parent chain, reported to Sensor.
#[derive(Debug, Clone)]
pub struct LineageInfo {
    pub parent_uid: u32,
    pub parent_exe_path: String,
}
