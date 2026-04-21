# Update Falcosecurity-Libs Fork

You are helping update the falcosecurity-libs fork used by the StackRox collector.

## Repository Context

- **Collector repo**: The current working directory
- **Fork submodule**: `falcosecurity-libs/` — StackRox's fork of `https://github.com/falcosecurity/libs`
- **Fork repo**: `https://github.com/stackrox/falcosecurity-libs`
- **Upstream remote** (in submodule): `falco` → `git@github.com:falcosecurity/libs`
- **Origin remote** (in submodule): `origin` → `git@github.com:stackrox/falcosecurity-libs.git`
- **Branch naming**: `X.Y.Z-stackrox` branches carry StackRox patches on top of upstream tags
- **Update docs**: `docs/falco-update.md`

## Step 1: Assess Current State

Run the following in the `falcosecurity-libs/` submodule:

1. `git describe --tags HEAD` — find current upstream base version
2. `git log --oneline <base_tag>..HEAD --no-merges` — list all StackRox patches
3. `git fetch falco --tags` — get latest upstream tags
4. `git tag -l '0.*' | sort -V | tail -10` — find latest upstream releases
5. `git branch -a | grep stackrox` — find existing StackRox branches
6. `git log --oneline <current_tag>..<target_tag> | wc -l` — count upstream commits
7. `git log --oneline HEAD --not --remotes=falco` — find StackRox-only patches

Report: current version, target version, number of StackRox patches, number of upstream commits.

## Step 2: Analyze StackRox Patches

For each StackRox patch, determine if it has been upstreamed:

```sh
git log --oneline <current_tag>..<target_tag> --grep="<keyword_from_patch>"
```

Categorize each patch as:
- **Upstreamed** — will be dropped automatically during rebase
- **Still needed** — must be carried forward
- **Conflict risk** — touches files heavily modified upstream

### Current StackRox Patches (as of 0.23.1-stackrox)

These are squashed into a single commit on the stackrox branch. Categories:

**BPF verifier fixes** (keep — not upstreamed):
- clang > 19 verifier fixes (MAX_IOVCNT, volatile len_to_read, pragma unroll)
- RHEL SAP verifier fix (const struct cred *)
- COS verifier fix (RCU pointer chain reads)
- explicit return in auxmap submit
- `sys_exit` null-check optimization (single `maps__get_capture_settings()` lookup)
- `volatile` on `push__bytebuf` `len_to_read` parameter (32-bit register bounds on kernel 4.18)
- `#if 0` stubs for `t1_execveat_x` / `t2_execveat_x` (collector doesn't subscribe to execveat; frees instruction budget for volatile fix)

**TOCTOU mitigation program disable** (keep):
- Disable autoloading of 64-bit TOCTOU programs whose syscalls don't exist on the running kernel (e.g., `openat2_e` on kernels < 5.6). Uses tracepoint existence check (`/sys/kernel/tracing/events/syscalls/sys_enter_<syscall>`) with debugfs fallback. In `lifecycle.c`.

**Exepath resolution** (keep):
- Fallback in `parse_execve_exit` using Parameter 31 (`bprm->filename`) from exit event when enter events are unavailable
- Filter fd-based execs (`/dev/fd/N`, `/proc/self/fd/N`) in the fallback — container runtimes use fexecve as an intermediate exec step

**ppc64le platform support** (keep):
- ppc64le vmlinux.h (large, BTF-generated)
- ppc64le syscall compat header
- IOC_PAGE_SHIFT fix
- thread_info guards

**Performance optimizations** (keep):
- cgroup subsys filtering (`INTERESTING_SUBSYS` compile flag)
- socket-only FD scan (`SCAP_SOCKET_ONLY_FD` compile flag)

**API/build adaptations** (keep):
- libelf suffix guard + initial filtercheck extract
- sinsp include directory fix
- CMake/include fixes for logging integration
- disable log timestamps API

**Workarounds** (keep but monitor):
- disable trusted exepath (see "Exepath" section below)
- ASSERT_TO_LOG via falcosecurity_log_fn callback

**Rebase fixups** (always regenerated):
- CMake cycle fix, exepath fallback, assert macro fixes

### Upstream Candidates

These patches are generic enough to propose upstream:
- **Strong**: clang verifier fixes, disable log timestamps, TOCTOU disable logic
- **With discussion**: cgroup filtering, socket-only FD scan — upstream may prefer runtime flags over compile-time
- **ppc64le bundle**: propose together if upstream is interested in the architecture

## Step 3: Identify Breaking API Changes

Check what APIs changed between versions:

```sh
# Thread/container changes
git log --oneline <current>..<target> -- userspace/libsinsp/threadinfo.h userspace/libsinsp/thread_manager.h

# sinsp API
git diff <current>..<target> -- userspace/libsinsp/sinsp.h | grep -E '^\+|^\-' | head -80

# Event format changes
git diff <current>..<target> -- driver/event_table.c

# Breaking changes
git log --oneline <current>..<target> --grep="BREAKING\|!:"
```

Then grep collector for uses of changed APIs:
```sh
grep -rn '<removed_api_name>' collector/lib/ collector/test/ --include='*.cpp' --include='*.h'
```

Key collector integration points:
- `collector/lib/system-inspector/Service.cpp` — sinsp init, filter setup (`proc.pid != proc.vpid`)
- `collector/lib/system-inspector/EventExtractor.h` — threadinfo field access macros
- `collector/lib/ProcessSignalFormatter.cpp` — process signals, exepath, container_id, lineage
- `collector/lib/NetworkSignalHandler.cpp` — container_id via `GetContainerID(evt)`
- `collector/lib/Process.cpp` — container_id via cgroup extraction
- `collector/lib/Utility.cpp` — `GetContainerID()`, `ExtractContainerIDFromCgroup()`
- `collector/test/ProcessSignalFormatterTest.cpp` — thread creation, thread_manager
- `collector/CMakeLists.txt` — falco build flags, `MODERN_BPF_EXCLUDE_PROGS`

## Step 4: Plan Staging Strategy

If the gap is large (>200 commits), identify intermediate stopping points at version boundaries where major API changes happen.

Known historical API breakpoints:
- **0.20.0**: `set_import_users` lost second arg, `m_user.uid()`/`m_group.gid()` → `m_uid`/`m_gid`
- **0.21.0**: Container engine removed. `m_container_id` removed from threadinfo. Enter events deprecated (`EF_OLD_VERSION`).
- **0.22.0**: `get_thread_ref` removed (use `find_thread`). `extract_single` API changed in filterchecks.
- **0.23.0+**: Parent traversal moved to thread_manager. `get_thread_info(bool)` → `get_thread_info()`. `m_user`/`m_group` structs removed.

## Step 5: Execute Rebase

```sh
cd falcosecurity-libs
git fetch falco
git switch upstream-main && git merge --ff-only falco/master && git push origin upstream-main --tags
git switch <previous_branch>
git switch -c <new_version>-stackrox
git rebase <new_version_tag>
# Resolve conflicts
git push -u origin <new_version>-stackrox
```

Always rebase onto upstream **tags** (not master tip) per `docs/falco-update.md`.

## Step 6: Update Collector Code

### Common patterns of change

**Container ID access** (from 0.21.0+):
- Container plugin NOT used — collector extracts IDs from thread cgroups
- `GetContainerID(sinsp_threadinfo&)` iterates `tinfo.cgroups()` → `ExtractContainerIDFromCgroup()` (Utility.cpp)
- sinsp filter: `proc.pid != proc.vpid` (built-in) instead of `container.id != 'host'` (plugin)

**Thread access** (from 0.22.0+):
- `get_thread_ref(tid, true)` → `m_thread_manager->get_thread(tid)`
- `get_thread_info(true)` → `get_thread_info()`

**User/group** (from 0.20.0+):
- `m_user.uid()` / `m_group.gid()` → `m_uid` / `m_gid`

**Thread creation in tests**:
- `build_threadinfo()` → `inspector->get_threadinfo_factory().create()`
- `add_thread()` → `inspector->m_thread_manager->add_thread(std::move(tinfo), false)`

**Lineage traversal** (from 0.23.0+):
- `mt->traverse_parent_state(visitor)` → `inspector_->m_thread_manager->traverse_parent_state(*mt, visitor)`
- `sinsp_threadinfo::visitor_func_t` → `sinsp_thread_manager::visitor_func_t`

**FilterCheck API** (from 0.22.0+):
- `extract_single(event, &len)` → `extract(event, vals)` vector-based
- Add null guards for `filter_check` pointers

## Step 7: Known Gotchas

### Exepath Resolution (CRITICAL)

Modern drivers **no longer send execve enter events** (`EF_OLD_VERSION`). The exepath should come from `trusted_exepath` (param 28), but StackRox **disables it** because it resolves symlinks (breaking busybox container policies).

**Without either source, `m_exepath` inherits the parent's value** (e.g., `/usr/bin/podman`).

**Fix**: Fallback in `parse_execve_exit` using Parameter 31 (`bprm->filename`):
```cpp
if(!exepath_set) {
    if(const auto filename_param = evt.get_param(30); !filename_param->empty()) {
        std::string_view filename = filename_param->as<std::string_view>();
        // Skip fd-based execs — intermediate container runtime steps
        if(filename != "<NA>" &&
           filename.substr(0, 8) != "/dev/fd/" &&
           filename.substr(0, 15) != "/proc/self/fd/") {
            std::string fullpath = sinsp_utils::concatenate_paths(
                evt.get_tinfo()->get_cwd(), filename);
            evt.get_tinfo()->set_exepath(std::move(fullpath));
        }
    }
}
```

The `/dev/fd/` filter is critical: container runtimes (runc/crun) use `fexecve()` (`/dev/fd/N`) as an intermediate exec step before the real entry point. Without the filter, phantom process signals appear with name like `"6"`, exepath `/dev/fd/6`, args `"init"`. This causes `TestProcessListeningOnPort` failures on platforms using this runtime pattern (rhcos-4.12, ppc64le).

**Detection**: `TestProcessViz` shows all processes with container runtime path. `TestProcessListeningOnPort` shows wrong process name/path.

### TOCTOU Mitigation Programs

TOCTOU mitigation programs are in `attached/events/toctou_mitigation/`, separate from regular `tail_called/` programs. They are **always autoloaded** — NOT covered by `MODERN_BPF_EXCLUDE_PROGS`.

If a TOCTOU program references kernel types that don't exist on the running kernel (e.g., `struct open_how` for `openat2` on kernels < 5.6), CO-RE relocation fails and the entire BPF skeleton fails to load.

**Fix**: In `lifecycle.c`, before the ia32 TOCTOU loop, add a loop that checks tracepoint existence for each 64-bit TOCTOU program:
```c
for(int i = 0; i < TTM_MAX; i++) {
    const char *prog_name = ttm_progs_table[i].ttm_64bit_prog.name;
    if(prog_name == NULL) continue;
    // Check /sys/kernel/tracing/events/syscalls/sys_enter_<syscall>
    // Strip "_e" suffix from prog name to build tracepoint path
    // Fallback to /sys/kernel/debug/tracing/... (debugfs mount)
    // If neither exists, call disable_prog_autoloading()
}
```

The ia32 programs already have disable logic (using `is_kernel_symbol_available()`), but the 64-bit programs previously assumed all syscalls exist.

### BPF Verifier Compatibility

**32-bit sub-register bounds (kernel 4.18)**: When clang emits `w2 = w8` (32-bit move), kernel 4.18's verifier loses the upper-bound tracking. A `uint16_t` with known max 15999 becomes unbounded after the move. `bpf_probe_read` rejects the size argument.

**Clang is smarter than the verifier**: `& 0xFFFF` on a `uint16_t` is a compile-time no-op — clang removes it. Even `unsigned long safe = len; safe &= 0xFFFF;` gets optimized away if clang can track that `len` was `uint16_t`. Only `volatile` prevents this.

**volatile tradeoff**: `volatile` increases instruction count. The `push__bytebuf` function is called from many programs. When making `len_to_read` volatile, verify that all calling programs stay under the 1M instruction limit. If needed, stub out unused programs with `#if 0` to free instruction budget (e.g., `t1_execveat_x`/`t2_execveat_x` when collector doesn't subscribe to execveat).

**Clang null-check elimination**: Multiple inlined calls to `bpf_map_lookup_elem()` on the same map — clang drops null checks after the first. Fix: single lookup in caller, pass pointer down.

Common fix patterns:
- **`volatile` qualifier**: Prevent clang from optimizing away bounds narrowing
- **Single lookup + pass pointer**: Prevent null-check elimination across inlined functions
- **`#pragma unroll`**: For loops the verifier can't bound
- **`const` on credential pointers**: For COS clang-compiled kernels with RCU attributes
- **Reduce loop bounds**: e.g., `MAX_IOVCNT` 32 → 16

### CMake Dependency Cycle

Upstream has a cyclic dependency that newer CMake (3.31+) rejects. Fix: compile the 3 required driver source files (`event_table.c`, `flags_table.c`, `dynamic_params_table.c`) directly into the generator. Lives in `driver/modern_bpf/CMakeLists.txt`.

### ASSERT_TO_LOG Circular Dependency

Collector uses `-DASSERT_TO_LOG`. Use `falcosecurity_log_fn` callback from `scap_log.h` (no dependency issues) instead of `libsinsp_logger()`.

### Container Plugin Not Used

The upstream container plugin (`libcontainer.so`) is NOT used. Container IDs come from cgroup extraction via `ExtractContainerIDFromCgroup()`. The sinsp filter uses `proc.pid != proc.vpid` (built-in) instead of `container.id != 'host'` (plugin-provided). No plugin loading, no Go worker, no container runtime dependency.

### Network Signal Handler: UDP Socket State

`sinsp::parse_rw_exit()` marks sockets "failed" on EAGAIN but never clears on success for recv. Fix in `NetworkSignalHandler.cpp`: skip `is_socket_failed()`/`is_socket_pending()` for send/recv events. Detection: UDP network flow tests fail for `recvfrom`/`recvmsg` with `SO_RCVTIMEO`.

## Step 8: Validate

### Build Commands

```bash
# From repo root (NOT collector/ subdirectory)
make image      # Build collector image
make unittest   # Run unit tests

# Integration tests (from integration-tests/ directory)
cd integration-tests
DOCKER_HOST=unix:///run/podman/podman.sock COLLECTOR_LOG_LEVEL=debug make TestProcessNetwork
DOCKER_HOST=unix:///run/podman/podman.sock COLLECTOR_LOG_LEVEL=debug make TestUdpNetworkFlow
```

### Validation Checklist

- [ ] `make image` succeeds
- [ ] `make unittest` passes
- [ ] Container ID attribution works (not all empty or host)
- [ ] Process exepaths correct (not showing `/usr/bin/podman`)
- [ ] No phantom `/dev/fd/N` processes in TestProcessListeningOnPort
- [ ] BPF probe loads on all CI platforms (check RHEL 8, COS, Ubuntu 22.04)
- [ ] No CO-RE relocation failures on kernels < 5.6

### Key Integration Tests

- **TestProcessNetwork**: Process ExePath, Name, Args, network connections, lineage
- **TestProcessListeningOnPort**: Process-to-endpoint attribution. Catches phantom exec issues.
- **TestUdpNetworkFlow**: UDP connection tracking (30s timeouts, `MAX_SENDMMSG_RECVMMSG_SIZE=16`)
- **TestDuplicateEndpoints**: Endpoint deduplication. Also catches phantom exec issues.
- **TestCollectorStartup**: Smoke test — catches BPF loading failures

### CI Kernel Compatibility Matrix

| Platform | Kernel | Verifier Strictness |
|---|---|---|
| RHEL 8 / RHCOS 4.12 / s390x / ppc64le | 4.18 | **Most restrictive** |
| RHEL 9 / RHEL SAP | 5.14 | Moderate (SAP can hit 1M insn limit) |
| COS | 6.6 | Clang-compiled kernel — different BTF |
| Ubuntu 22.04 | 6.8 | Moderate |
| Ubuntu 24.04 | 6.17 | Permissive |
| Fedora CoreOS / Flatcar | latest | Most permissive |

## Step 9: Final Update

```sh
cd <collector_repo_root>
cd falcosecurity-libs && git checkout <final_version>-stackrox
cd .. && git add falcosecurity-libs
```

Update `docs/falco-update.md` with version transition, new patches, dropped patches, known issues.

## PR Strategy

Each update should produce **two PRs**:
1. **Fork PR** targeting `upstream-main` in `stackrox/falcosecurity-libs`
2. **Collector PR** updating the submodule pointer and collector-side code

## Quick Reference: Event Architecture

### How Process Events Flow

1. **Kernel BPF** (`sched_process_exec`) → ring buffer
2. **libscap** reads ring buffer → `scap_evt`
3. **libsinsp parsers** (`parsers.cpp`) → updates thread info
4. **Collector** (`ProcessSignalFormatter`) → reads via `EventExtractor` → sends to sensor

### Key Thread Info Fields

| Field | Source | Notes |
|-------|--------|-------|
| `m_comm` | Exit event param 13 | Always correct |
| `m_exe` | Exit event param 1 | argv[0], may be relative |
| `m_exepath` | Param 27 (trusted, disabled) or param 30 (bprm->filename fallback) | See Exepath gotcha |
| `m_pid` | Exit event param 4 | |
| `m_uid`/`m_gid` | Exit event param 26/29 | Was `m_user.uid()`/`m_group.gid()` |
| container_id | Extracted from cgroups via `GetContainerID()` | Was `m_container_id` |

### PPME_SYSCALL_EXECVE_19_X Parameters (0-indexed)

0: res, 1: exe, 2: args, 3: tid, 4: pid, 5: ptid, 6: cwd, 13: comm, 14: cgroups, 15: env, 26: uid, 27: trusted_exepath (disabled), 28: pgid, 29: gid, 30: filename (bprm->filename — used by exepath fallback)

## Step 10: Update This Skill

**Mandatory.** After completing an update, review and update this file with:
- New API breakpoints (Step 4)
- Updated StackRox patches list (Step 2)
- New gotchas (Step 7)
- CI matrix changes (Step 8)
- Corrected or removed outdated information
