use std::collections::HashMap;
use std::time::Instant;

use collector_types::container::ContainerId;
use collector_types::process::{LineageInfo, ProcessInfo};

const DEFAULT_MAX_SIZE: usize = 32768;
const DEFAULT_MAX_LINEAGE_DEPTH: usize = 10;

struct ProcessEntry {
    info: ProcessInfo,
    ppid: u32,
    last_seen: Instant,
}

pub struct ProcessTable {
    entries: HashMap<u32, ProcessEntry>,
    max_size: usize,
}

impl ProcessTable {
    pub fn new() -> Self {
        Self {
            entries: HashMap::new(),
            max_size: DEFAULT_MAX_SIZE,
        }
    }

    pub fn with_max_size(max_size: usize) -> Self {
        Self {
            entries: HashMap::new(),
            max_size,
        }
    }

    pub fn upsert(&mut self, info: ProcessInfo, ppid: u32) -> Option<ProcessInfo> {
        self.upsert_at(info, ppid, Instant::now())
    }

    fn upsert_at(&mut self, info: ProcessInfo, ppid: u32, now: Instant) -> Option<ProcessInfo> {
        let pid = info.pid;

        if self.entries.len() >= self.max_size && !self.entries.contains_key(&pid) {
            self.evict_oldest();
        }

        let old = self.entries.insert(
            pid,
            ProcessEntry {
                info,
                ppid,
                last_seen: now,
            },
        );

        old.map(|e| e.info)
    }

    pub fn remove(&mut self, pid: u32) -> Option<ProcessInfo> {
        self.entries.remove(&pid).map(|e| e.info)
    }

    pub fn get(&self, pid: u32) -> Option<&ProcessInfo> {
        self.entries.get(&pid).map(|e| &e.info)
    }

    pub fn lineage(&self, pid: u32, container_id: &ContainerId) -> Vec<LineageInfo> {
        self.lineage_with_depth(pid, container_id, DEFAULT_MAX_LINEAGE_DEPTH)
    }

    pub fn lineage_with_depth(
        &self,
        pid: u32,
        container_id: &ContainerId,
        max_depth: usize,
    ) -> Vec<LineageInfo> {
        let mut result = Vec::new();
        let mut current_pid = pid;
        let mut last_exe: Option<&str> = None;

        for _ in 0..max_depth {
            let entry = match self.entries.get(&current_pid) {
                Some(e) => e,
                None => break,
            };

            let parent_pid = entry.ppid;
            let parent = match self.entries.get(&parent_pid) {
                Some(p) => p,
                None => break,
            };

            // Stop at container boundary
            if parent.info.container_id != *container_id {
                break;
            }

            // Collapse consecutive same exe_path
            let should_add = match last_exe {
                Some(prev) => prev != parent.info.exe_path,
                None => true,
            };

            if should_add {
                result.push(LineageInfo {
                    parent_uid: parent.info.uid,
                    parent_exe_path: parent.info.exe_path.clone(),
                });
                last_exe = Some(&parent.info.exe_path);
            }

            current_pid = parent_pid;
            // Stop if parent is pid 0 or self-referential
            if parent_pid == 0 || parent_pid == current_pid {
                break;
            }
        }

        result
    }

    pub fn iter(&self) -> impl Iterator<Item = &ProcessInfo> {
        self.entries.values().map(|e| &e.info)
    }

    fn evict_oldest(&mut self) {
        if let Some((&oldest_pid, _)) = self
            .entries
            .iter()
            .min_by_key(|(_, e)| e.last_seen)
        {
            self.entries.remove(&oldest_pid);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_process(pid: u32, exe: &str, container: &str) -> ProcessInfo {
        ProcessInfo {
            pid,
            uid: 0,
            gid: 0,
            exe_path: exe.to_string(),
            args: String::new(),
            container_id: ContainerId(container.to_string()),
        }
    }

    #[test]
    fn lineage_stops_at_container_boundary() {
        let mut table = ProcessTable::new();
        let cid = ContainerId("container1".into());

        // pid 1 is in a different container (host)
        table.upsert(make_process(1, "/sbin/init", "host"), 0);
        // pid 100 is in container1, parent is pid 1
        table.upsert(make_process(100, "/bin/bash", "container1"), 1);
        // pid 200 is in container1, parent is pid 100
        table.upsert(make_process(200, "/usr/bin/curl", "container1"), 100);

        let lineage = table.lineage(200, &cid);
        // Should only include pid 100 (/bin/bash), not pid 1 (different container)
        assert_eq!(lineage.len(), 1);
        assert_eq!(lineage[0].parent_exe_path, "/bin/bash");
    }

    #[test]
    fn lineage_collapses_consecutive_same_exe() {
        let mut table = ProcessTable::new();
        let cid = ContainerId("c1".into());

        table.upsert(make_process(1, "/bin/bash", "c1"), 0);
        table.upsert(make_process(2, "/bin/bash", "c1"), 1);
        table.upsert(make_process(3, "/bin/bash", "c1"), 2);
        table.upsert(make_process(4, "/usr/bin/app", "c1"), 3);

        let lineage = table.lineage(4, &cid);
        // Three consecutive /bin/bash should collapse to one
        assert_eq!(lineage.len(), 1);
        assert_eq!(lineage[0].parent_exe_path, "/bin/bash");
    }

    #[test]
    fn lineage_max_depth() {
        let mut table = ProcessTable::new();
        let cid = ContainerId("c1".into());

        // Create a chain deeper than max depth
        for i in 1..=15 {
            table.upsert(
                make_process(i, &format!("/bin/p{i}"), "c1"),
                i.saturating_sub(1),
            );
        }

        let lineage = table.lineage_with_depth(15, &cid, 3);
        assert!(lineage.len() <= 3);
    }

    #[test]
    fn eviction_removes_oldest() {
        let mut table = ProcessTable::with_max_size(2);
        let t0 = Instant::now();
        let t1 = t0 + std::time::Duration::from_secs(1);
        let t2 = t0 + std::time::Duration::from_secs(2);

        table.upsert_at(make_process(1, "/bin/a", "c1"), 0, t0);
        table.upsert_at(make_process(2, "/bin/b", "c1"), 0, t1);
        // This should evict pid 1 (oldest)
        table.upsert_at(make_process(3, "/bin/c", "c1"), 0, t2);

        assert!(table.get(1).is_none());
        assert!(table.get(2).is_some());
        assert!(table.get(3).is_some());
    }

    #[test]
    fn iter_returns_all_processes() {
        let mut table = ProcessTable::new();
        table.upsert(make_process(1, "/bin/a", "c1"), 0);
        table.upsert(make_process(2, "/bin/b", "c1"), 0);
        table.upsert(make_process(3, "/bin/c", "c1"), 0);

        let pids: Vec<u32> = table.iter().map(|p| p.pid).collect();
        assert_eq!(pids.len(), 3);
        assert!(pids.contains(&1));
        assert!(pids.contains(&2));
        assert!(pids.contains(&3));
    }

    #[test]
    fn remove_returns_process() {
        let mut table = ProcessTable::new();
        table.upsert(make_process(42, "/bin/test", "c1"), 0);
        let removed = table.remove(42);
        assert!(removed.is_some());
        assert_eq!(removed.unwrap().pid, 42);
        assert!(table.get(42).is_none());
    }
}
