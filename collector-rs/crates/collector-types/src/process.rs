use crate::container::ContainerId;

#[derive(Debug, Clone)]
pub struct ProcessInfo {
    pub pid: u32,
    pub uid: u32,
    pub gid: u32,
    pub exe_path: String,
    pub args: String,
    pub container_id: ContainerId,
}

#[derive(Debug, Clone)]
pub struct LineageInfo {
    pub parent_uid: u32,
    pub parent_exe_path: String,
}
