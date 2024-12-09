#ifndef COLLECTOR_PROCFSSCRAPER_INTERNAL_H
#define COLLECTOR_PROCFSSCRAPER_INTERNAL_H

#include <optional>
#include <string_view>

namespace collector {

// ExtractContainerID tries to extract a container ID from a cgroup line.
std::optional<std::string_view> ExtractContainerID(std::string_view cgroup_line);

// ExtractProcessState retrieves the state of the process (3rd element).
// as found in /proc/<pid>/stat.
// Returns: the state character or nullopt in case of error.
std::optional<char> ExtractProcessState(std::string_view proc_pid_stat_line);

}  // namespace collector

#endif
