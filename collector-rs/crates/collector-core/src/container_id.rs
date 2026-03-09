const CONTAINER_ID_LENGTH: usize = 64;
const SHORT_CONTAINER_ID_LENGTH: usize = 12;

fn is_container_id(s: &str) -> bool {
    s.len() == CONTAINER_ID_LENGTH && s.bytes().all(|b| b.is_ascii_hexdigit())
}

/// Extracts a 12-char container ID from a cgroup path.
///
/// Handles Docker (/docker/<64hex>), CRI-O (crio-<64hex>.scope),
/// containerd (cri-containerd-<64hex>.scope), systemd (docker-<64hex>.scope),
/// kubepods paths, and bare 64-hex path segments.
///
/// Matches the C++ ExtractContainerIDFromCgroup logic in Utility.cpp.
pub fn extract_container_id(cgroup: &str) -> Option<&str> {
    // Handle bare 64-hex cgroup names (from kernfs_node name in BPF)
    if is_container_id(cgroup) {
        return Some(&cgroup[..SHORT_CONTAINER_ID_LENGTH]);
    }

    if cgroup.len() < CONTAINER_ID_LENGTH + 1 {
        return None;
    }

    // Strip .scope suffix if present
    let cgroup = match cgroup.rfind(".scope") {
        Some(pos) => {
            let trimmed = &cgroup[..pos];
            if trimmed.len() < CONTAINER_ID_LENGTH + 1 {
                return None;
            }
            trimmed
        }
        None => cgroup,
    };

    // The last 65 chars should be separator + 64-hex-char ID
    let id_start = cgroup.len() - (CONTAINER_ID_LENGTH + 1);
    let separator = cgroup.as_bytes()[id_start];
    if separator != b'/' && separator != b'-' {
        return None;
    }

    // Exclude conmon containers
    let prefix = &cgroup[..id_start];
    if prefix.ends_with("-conmon") {
        return None;
    }

    let full_id = &cgroup[id_start + 1..];
    if !is_container_id(full_id) {
        return None;
    }

    Some(&full_id[..SHORT_CONTAINER_ID_LENGTH])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn docker_cgroup() {
        let cgroup = "/docker/abc123def456abc123def456abc123def456abc123def456abc123def456abcd";
        assert_eq!(extract_container_id(cgroup), Some("abc123def456"));
    }

    #[test]
    fn crio_cgroup() {
        let cgroup = "/kubepods/besteffort/pod1234/crio-abc123def456abc123def456abc123def456abc123def456abc123def456abcd.scope";
        assert_eq!(extract_container_id(cgroup), Some("abc123def456"));
    }

    #[test]
    fn containerd_cgroup() {
        let cgroup = "/system.slice/cri-containerd-abc123def456abc123def456abc123def456abc123def456abc123def456abcd.scope";
        assert_eq!(extract_container_id(cgroup), Some("abc123def456"));
    }

    #[test]
    fn systemd_docker_cgroup() {
        let cgroup = "/system.slice/docker-abc123def456abc123def456abc123def456abc123def456abc123def456abcd.scope";
        assert_eq!(extract_container_id(cgroup), Some("abc123def456"));
    }

    #[test]
    fn kubepods_slash_separated() {
        let cgroup = "/kubepods/besteffort/podabc/abc123def456abc123def456abc123def456abc123def456abc123def456abcd";
        assert_eq!(extract_container_id(cgroup), Some("abc123def456"));
    }

    #[test]
    fn host_process_returns_none() {
        assert_eq!(extract_container_id("/"), None);
        assert_eq!(extract_container_id("/init.scope"), None);
        assert_eq!(extract_container_id("/user.slice/user-1000.slice"), None);
    }

    #[test]
    fn short_string_returns_none() {
        assert_eq!(extract_container_id(""), None);
        assert_eq!(extract_container_id("abc"), None);
    }

    #[test]
    fn non_hex_id_returns_none() {
        // 64 chars but not all hex
        let bad = "/docker/gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg";
        assert_eq!(extract_container_id(bad), None);
    }

    #[test]
    fn conmon_excluded() {
        let cgroup = "/kubepods/pod123/crio-conmon-abc123def456abc123def456abc123def456abc123def456abc123def456abcd.scope";
        assert_eq!(extract_container_id(cgroup), None);
    }

    #[test]
    fn wrong_separator_returns_none() {
        // Use . as separator instead of / or -
        let cgroup = "/kubepods.abc123def456abc123def456abc123def456abc123def456abc123def456abcd";
        assert_eq!(extract_container_id(cgroup), None);
    }

    #[test]
    fn bare_64_hex_from_bpf_kernfs() {
        // BPF reads kernfs_node name which is the bare 64-hex container ID
        let cgroup = "abc123def456abc123def456abc123def456abc123def456abc123def456abcd";
        assert_eq!(extract_container_id(cgroup), Some("abc123def456"));
    }

    #[test]
    fn bare_64_hex_uppercase() {
        let cgroup = "ABC123DEF456ABC123DEF456ABC123DEF456ABC123DEF456ABC123DEF456ABCD";
        assert_eq!(extract_container_id(cgroup), Some("ABC123DEF456"));
    }

    #[test]
    fn bare_63_chars_returns_none() {
        // One char short of a container ID
        let cgroup = "abc123def456abc123def456abc123def456abc123def456abc123def456abc";
        assert_eq!(extract_container_id(cgroup), None);
    }

    #[test]
    fn bare_65_chars_returns_none() {
        // One char too long
        let cgroup = "abc123def456abc123def456abc123def456abc123def456abc123def456abcde";
        assert_eq!(extract_container_id(cgroup), None);
    }
}
