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
6. Count upstream commits: `git log --oneline <current_tag>..<target_tag> | wc -l`
7. Find StackRox-only patches: `git log --oneline HEAD --not --remotes=falco`

Report: current version, target version, number of StackRox patches, number of upstream commits.

## Step 2: Analyze StackRox Patches

For each StackRox patch, determine if it has been upstreamed:

```sh
# For each patch commit, search upstream for equivalent
git log --oneline <current_tag>..<target_tag> --grep="<keyword_from_patch>"
```

Categorize each patch as:
- **Upstreamed** — will be dropped automatically during rebase
- **Still needed** — must be carried forward
- **Conflict risk** — touches files heavily modified upstream

### Current StackRox Patches (as of 0.23.1-stackrox-rc1)

20 patches in these categories:

**BPF verifier fixes** (keep — not upstreamed):
- `2291f61ec` — clang > 19 verifier fixes (MAX_IOVCNT, volatile len_to_read, pragma unroll)
- `8672099d6` — RHEL SAP verifier fix (const struct cred *)
- `df93a9e42` — COS verifier fix (RCU pointer chain reads)
- `d1a708bde` — explicit return in auxmap submit

**ppc64le platform support** (keep):
- `255126d47` — ppc64le vmlinux.h (large, BTF-generated)
- `a9cafe949` — ppc64le syscall compat header
- `452679e2b` — IOC_PAGE_SHIFT fix
- `dd5e86d40` / `bb733f64a` — thread_info guards (iterative, consider squashing)

**Performance optimizations** (keep):
- `a982809e0` — cgroup subsys filtering (`INTERESTING_SUBSYS` compile flag)
- `8dd26e3dc` — socket-only FD scan (`SCAP_SOCKET_ONLY_FD` compile flag)

**API/build adaptations** (keep):
- `32f36f770` — expose `extract_single` in filterchecks (public API)
- `b0ec4099f` — libelf suffix guard + initial filtercheck extract
- `34d863440` — sinsp include directory fix
- `a915789ec` / `16edb6bb1` — CMake/include fixes for logging integration
- `5338014a7` — disable log timestamps API

**Workarounds** (keep but monitor):
- `8ba291e78` — disable trusted exepath (see "Exepath" section below)
- `88d5093f4` — ASSERT_TO_LOG via falcosecurity_log_fn callback

**BPF verifier null-check optimization** (keep — not upstreamed):
- BPF verifier fix for `sys_exit` program: refactored `sampling_logic_exit()` and `sys_exit()` in `syscall_exit.bpf.c` to use a single `maps__get_capture_settings()` lookup instead of multiple inlined calls that clang optimizes into null-unsafe code. Without this, the BPF probe fails to load on kernels < 6.17 (RHEL 9, Ubuntu 22.04, COS, etc.)

**Network signal handler fix** (keep — collector-side):
- Skip `is_socket_failed()`/`is_socket_pending()` checks for send/recv events in `NetworkSignalHandler.cpp`. The sinsp parser marks sockets as "failed" on EAGAIN but never clears the flag on subsequent success for recv operations.

**Rebase fixups** (always regenerated):
- `d0fb1702c` — fixes following rebase (CMake cycle, exepath fallback, assert macro)

### Upstream Candidates

These patches are generic enough to propose upstream:
- **Strong**: clang verifier fixes (2291f61ec, 8672099d6, df93a9e42), disable log timestamps (5338014a7)
- **With discussion**: cgroup filtering (a982809e0), socket-only FD scan (8dd26e3dc), log asserts (88d5093f4) — upstream may prefer runtime flags over compile-time
- **ppc64le bundle**: propose together if upstream is interested in the architecture

## Step 3: Identify Breaking API Changes

Check what APIs changed between versions. Key areas to inspect:

```sh
# Thread cgroup / container-related changes (collector uses cgroup extraction, not the container plugin)
git log --oneline <current>..<target> -- userspace/libsinsp/threadinfo.h
git log --oneline <current>..<target> --grep="cgroup\|container"

# Thread manager changes
git log --oneline <current>..<target> -- userspace/libsinsp/threadinfo.h userspace/libsinsp/thread_manager.h

# sinsp API changes
git diff <current>..<target> -- userspace/libsinsp/sinsp.h | grep -E '^\+|^\-' | head -80

# Event format changes (parameter additions/removals)
git diff <current>..<target> -- driver/event_table.c

# Enter event deprecation (EF_OLD_VERSION flags)
git log --oneline <current>..<target> --grep="OLD_VERSION\|enter event\|enter_event"

# Breaking changes
git log --oneline <current>..<target> --grep="BREAKING\|breaking\|!:"
```

Then grep the collector code for uses of changed/removed APIs:

```sh
grep -rn '<removed_api_name>' collector/lib/ collector/test/ --include='*.cpp' --include='*.h'
```

Key collector integration points to check:
- `collector/lib/system-inspector/Service.cpp` — sinsp initialization, filter setup (`proc.pid != proc.vpid`)
- `collector/lib/system-inspector/EventExtractor.h` — threadinfo field access macros (TINFO_FIELD, FIELD_RAW, FIELD_RAW_SAFE)
- `collector/lib/ProcessSignalFormatter.cpp` — process signal creation, exepath access, container_id, lineage traversal
- `collector/lib/NetworkSignalHandler.cpp` — container_id access via `GetContainerID(evt)`
- `collector/lib/Process.cpp` — process info access, container_id via cgroup extraction
- `collector/lib/Utility.cpp` — `GetContainerID()`, `ExtractContainerIDFromCgroup()`, threadinfo printing
- `collector/test/ProcessSignalFormatterTest.cpp` — thread creation, thread_manager usage
- `collector/test/SystemInspectorServiceTest.cpp` — service initialization
- `collector/CMakeLists.txt` — falco build flags

## Step 4: Plan Staging Strategy

If the gap is large (>200 commits), identify intermediate stopping points:

1. Look for version boundaries where major API changes happen
2. Prefer stopping at versions where container/thread APIs change
3. Each stage should be independently buildable and testable

Known historical API breakpoints (update as upstream evolves):
- **0.20.0**: `set_import_users` lost second arg, user/group structs on threadinfo replaced with `m_uid`/`m_gid`
- **0.21.0**: Container engine subsystem removed entirely. `m_container_id` removed from threadinfo (collector uses cgroup extraction instead of upstream's container plugin). `m_thread_manager` changed to `shared_ptr`. `build_threadinfo()`/`add_thread()` removed from sinsp. Enter events for many syscalls deprecated (`EF_OLD_VERSION`).
- **0.22.0**: `get_thread_ref` removed from sinsp (use `find_thread`). `get_container_id()` removed from threadinfo. `extract_single` API changed in filterchecks.
- **0.23.0+**: Parent thread traversal moved to thread_manager. `get_thread_info(bool)` signature changed to `get_thread_info()` (no bool). `m_user`/`m_group` structs removed (use `m_uid`/`m_gid` directly).

## Step 5: Execute Rebase (per stage)

```sh
cd falcosecurity-libs
git fetch falco
git switch upstream-main && git merge --ff-only falco/master && git push origin upstream-main --tags
git switch <previous_branch>
git switch -c <new_version>-stackrox
git rebase <new_version_tag>
# Resolve conflicts using categorization from Step 2
# For each conflict: check if patch is still needed, compare against upstream equivalent
git push -u origin <new_version>-stackrox
```

Always rebase onto upstream **tags** (not master tip) per `docs/falco-update.md`.

## Step 6: Update Collector Code

After each rebase stage, update collector code for API changes found in Step 3.

### Common patterns of change

**Container ID access** (from 0.21.0+):
- Container plugin (`libcontainer.so`) is NOT used — collector extracts container IDs directly from thread cgroups
- `GetContainerID(sinsp_threadinfo&)` iterates `tinfo.cgroups()` and calls `ExtractContainerIDFromCgroup()` (Utility.cpp)
- `GetContainerID(sinsp_evt*)` extracts from event's thread info cgroups
- sinsp filter uses `proc.pid != proc.vpid` (built-in field) instead of `container.id != 'host'` (plugin field)
- No plugin loading, no `libcontainer.so`, no Go worker dependency
- `ContainerMetadata` class was removed — namespace/label lookup is not available without the plugin
- `ContainerInfoInspector` endpoint (`/state/containers/:id`) still exists but always returns empty namespace

**Thread access** (from 0.22.0+):
- Replace `get_thread_ref(tid, true)` with `m_thread_manager->find_thread(tid, false)` or `m_thread_manager->get_thread(tid, false)`
- `get_thread_info(true)` → `get_thread_info()` (no bool parameter)

**User/group** (from 0.20.0+):
- Replace `m_user.uid()` / `m_group.gid()` with `m_uid` / `m_gid`

**Thread creation in tests**:
- Replace `build_threadinfo()` with `inspector->get_threadinfo_factory().create()`
- Replace `add_thread()` with `inspector->m_thread_manager->add_thread(std::move(tinfo), false)`

**Lineage traversal** (from 0.23.0+):
- Replace `mt->traverse_parent_state(visitor)` with `inspector_->m_thread_manager->traverse_parent_state(*mt, visitor)`
- Visitor type: `sinsp_thread_manager::visitor_func_t` instead of `sinsp_threadinfo::visitor_func_t`

**FilterCheck API** (from 0.22.0+):
- `extract_single(event, &len)` → `extract(event, vals)` vector-based API
- Add null guards for `filter_check` pointers (plugin-provided checks may not be initialized)

**UDP test adjustments** (from 0.23.0+):
- UDP tests need 30-second timeouts (vs 5-10s for TCP) due to BPF event delivery pipeline latency
- `TestMultipleDestinations`: sendmmsg message count × server count must not exceed `MAX_SENDMMSG_RECVMMSG_SIZE` (16)
- File: `integration-tests/suites/udp_networkflow.go`

## Step 7: Known Gotchas

### Exepath Resolution (CRITICAL)

Modern drivers (0.21.0+) **no longer send execve enter events** (marked `EF_OLD_VERSION`). The exepath is supposed to come from the `trusted_exepath` parameter (param 28) in the exit event, which uses the kernel's `d_path()`.

However, the StackRox fork **disables trusted_exepath** (`USE_TRUSTED_EXEPATH=false`) because it resolves symlinks — giving `/bin/busybox` instead of `/bin/ls` in busybox containers, breaking ACS policies.

**Without either source, `m_exepath` inherits the parent's value on clone** (e.g., `/usr/bin/podman`), causing all container processes to show the container runtime's path.

**Fix**: Add a fallback in `parse_execve_exit` (parsers.cpp) that uses **Parameter 31** (`filename`, which is `bprm->filename`) from the exit event. This contains the first argument to execve as provided by the caller — same behavior as the old enter event reconstruction:

```cpp
// After the retrieve_enter_event() block, add:
if(!exepath_set) {
    /* Parameter 31: filename (type: PT_FSPATH) */
    if(const auto filename_param = evt.get_param(30); !filename_param->empty()) {
        std::string_view filename = filename_param->as<std::string_view>();
        if(filename != "<NA>") {
            std::string fullpath = sinsp_utils::concatenate_paths(
                evt.get_tinfo()->get_cwd(), filename);
            evt.get_tinfo()->set_exepath(std::move(fullpath));
        }
    }
}
```

**How to detect this bug**: Integration test `TestProcessViz` fails with all processes showing the container runtime binary (e.g., `/usr/bin/podman`) as their ExePath.

**Key event parameters** (PPME_SYSCALL_EXECVE_19_X, 0-indexed):
- 1: exe (argv[0]), 6: cwd, 13: comm (always correct)
- 27: trusted_exepath (kernel d_path, resolves symlinks — disabled)
- 30: filename (bprm->filename, first arg to execve — use this)

### CMake Dependency Cycle

Upstream has a cyclic dependency: `events_dimensions_generator → scap_event_schema → scap → pman → ProbeSkeleton → EventsDimensions → generator`. Upstream doesn't hit it because their CI uses CMake 3.22; our builder uses 3.31+ which enforces cycle detection.

**Fix**: Compile the 3 required driver source files (`event_table.c`, `flags_table.c`, `dynamic_params_table.c`) directly into the generator instead of linking `scap_event_schema`. This fix lives in `driver/modern_bpf/CMakeLists.txt` and must be carried forward each rebase.

### ASSERT_TO_LOG Circular Dependency

Collector compiles with `-DASSERT_TO_LOG` so assertions log instead of aborting. The old approach using `libsinsp_logger()` causes circular includes because `logger.h` includes `sinsp_public.h`.

**Fix**: Use `falcosecurity_log_fn` callback from `scap_log.h` (same pattern as `scap_assert.h`). This is a tiny header with no dependencies. The callback is set by sinsp when it opens the scap handle.

### Container Plugin Not Used

The upstream container plugin (`libcontainer.so`) is NOT used by collector. Container IDs are extracted directly from thread cgroups via `ExtractContainerIDFromCgroup()` in `Utility.cpp`. This avoids the Go worker dependency, CGO bridge, container runtime dependency, startup race conditions, and silent event-dropping failure modes of the plugin. The sinsp filter uses `proc.pid != proc.vpid` (built-in) instead of `container.id != 'host'` (plugin-provided). If a future falcosecurity-libs update changes cgroup format or thread API, update `ExtractContainerIDFromCgroup()` and `GetContainerID()` in Utility.cpp.

### BPF Verifier Compatibility

BPF verifier behavior varies significantly across:
- **Kernel versions**: Older kernels have stricter limits (see kernel matrix below)
- **Clang versions**: clang > 19 can produce code that exceeds instruction counts
- **Platform kernels**: RHEL SAP, Google COS have custom verifiers or clang-compiled kernels with different BTF attributes

**The most insidious class of bug**: Clang inlines `__always_inline` BPF helper functions and optimizes away null checks that the BPF verifier requires. This happens when:
1. Multiple inlined functions each call `bpf_map_lookup_elem()` on the same map
2. The compiler deduces from the first successful lookup that subsequent lookups can't return NULL
3. It removes the null check, but the verifier tracks each lookup independently as `map_value_or_null`
4. Result: `R0 invalid mem access 'map_value_or_null'` on older kernels

**Example** (found in 0.23.1): `syscall_exit.bpf.c:sampling_logic_exit()` called `maps__get_dropping_mode()` then `maps__get_sampling_ratio()` — both inlined functions that do `bpf_map_lookup_elem(&capture_settings, &key)`. Clang kept the null check for the first but dropped it for the second. Fix: do a single `maps__get_capture_settings()` call and access fields directly.

**Fix applied**: Refactored `sys_exit` BPF program to do one `maps__get_capture_settings()` lookup in the caller, pass the pointer to `sampling_logic_exit()`, and reuse it for `drop_failed` check. No redundant map lookups = no optimized-away null checks.

Common fix patterns:
- **Single lookup + direct field access**: Call the map lookup once, pass the pointer, access fields directly (preferred)
- **`volatile` qualifier**: Mark map lookup result as `volatile` to prevent optimization
- **Compiler barriers**: `asm volatile("")` after null check
- **Reduce loop bounds**: e.g., `MAX_IOVCNT` 32 → 16
- **`#pragma unroll`**: For loops the verifier can't bound
- **Break pointer chains**: Read through intermediate variables with null checks (e.g., `task->cred` on COS where kernel is clang-compiled with RCU attributes)
- **`const` qualifiers**: On credential struct pointers

### CI Kernel Compatibility Matrix

The BPF probe must load on all CI platforms. After each update, verify against this matrix:

| Platform | Kernel | Notes |
|---|---|---|
| Fedora CoreOS | 6.18+ | Newest kernel, most permissive verifier |
| Ubuntu 24.04 | 6.17+ | GCP VM, works with modern BPF |
| Ubuntu 22.04 | 6.8 | GCP VM, stricter verifier — **common failure point** |
| COS stable | 6.6 | Google kernel, clang-compiled — RCU/BTF differences |
| RHEL 9 | 5.14 | Oldest supported kernel — **most restrictive verifier** |
| RHEL SAP | 5.14 | Same kernel as RHEL 9 but different config |
| Flatcar | varies | Container Linux |
| ARM64 variants | varies | rhcos-arm64, cos-arm64, ubuntu-arm, fcarm |
| s390x | varies | rhel-s390x |
| ppc64le | varies | rhel-ppc64le |

**ubuntu-os** CI job runs on BOTH Ubuntu 22.04 AND 24.04 VMs. A failure on either fails the whole job.

**How to diagnose BPF loading failures from CI**:
1. Download the logs artifact (e.g., `ubuntu-os-logs`) from the GitHub Actions run
2. Find `collector.log` under `container-logs/<vm>/core-bpf/<TestName>/`
3. Search for `failed to load` — the line before it shows the verifier error
4. The verifier log shows exact instruction and register state at the point of rejection
5. Compare against master's CI run to confirm it's a regression

### Network Signal Handler: UDP send/recv Socket State (CRITICAL)

**Problem**: `sinsp::parse_rw_exit()` marks socket fd as "failed" (`set_socket_failed()`) when ANY send/recv syscall returns negative (e.g., EAGAIN from timeout). Unlike `connect()`, the success path for send/recv does NOT call `set_socket_connected()` to clear the flag. Result: once a UDP socket gets a single EAGAIN (common with `SO_RCVTIMEO`), all subsequent events on that fd are rejected by `GetConnection()`.

**Fix applied** in `collector/lib/NetworkSignalHandler.cpp`: Skip `is_socket_failed()` / `is_socket_pending()` checks for send/recv events (identified by `strncmp(evt_name, "send", 4)` or `strncmp(evt_name, "recv", 4)`). These checks are only relevant for TCP connection establishment (connect/accept/getsockopt).

**How to detect**: UDP network flow tests fail — connections from containers using `recvfrom`/`recvmsg`/`recvmmsg` with `SO_RCVTIMEO` are never reported. The server's receive call times out → EAGAIN → fd marked failed → all subsequent successful receives ignored.

## Step 8: Validate Each Stage

### Build Commands

```bash
# Build collector image (from repo root, NOT from collector/ subdirectory)
make image

# Run unit tests
make unittest

# Run specific integration test (from integration-tests/ directory)
cd integration-tests
DOCKER_HOST=unix:///run/podman/podman.sock COLLECTOR_LOG_LEVEL=debug make TestProcessNetwork
DOCKER_HOST=unix:///run/podman/podman.sock COLLECTOR_LOG_LEVEL=debug make TestUdpNetworkFlow
```

**Important**: Use `make image` from the repo root. Do NOT use `make -C collector image` — there is no `image` target in the collector subdirectory Makefile.

### Validation Checklist

Run this checklist after each stage:

- [ ] `falcosecurity-libs` builds via cmake `add_subdirectory`
- [ ] Each surviving patch verified: diff against original to ensure no content loss
- [ ] `make image` succeeds on amd64 (builds collector binary + container image)
- [ ] `make unittest` passes (all test suites, especially ProcessSignalFormatterTest)
- [ ] Integration tests pass (see key tests below)
- [ ] Multi-arch compilation: arm64, ppc64le, s390x
- [ ] Container ID attribution works via cgroup extraction (not all showing empty or host)
- [ ] Process exepaths are correct (not showing container runtime binary like `/usr/bin/podman`)
- [ ] Network signal handler receives correct container IDs
- [ ] Runtime self-checks pass
- [ ] BPF probe loads on older kernels (check CI results for RHEL 9, Ubuntu 22.04, COS)

### Key Integration Tests

- **TestProcessNetwork** (TestProcessViz + TestNetworkFlows + TestProcessLineageInfo):
  - Verifies process ExePath, Name, Args for container processes
  - Catches exepath regression (all paths show container runtime)
  - Verifies network connections attributed to correct containers
  - Verifies parent process lineage chains stop at container boundaries
- **TestUdpNetworkFlow**: Verifies UDP connection tracking across all send/recv syscall combinations:
  - Tests: sendto, sendmsg, sendmmsg × recvfrom, recvmsg, recvmmsg (9 combinations)
  - TestMultipleDestinations: one client → multiple servers (watch `MAX_SENDMMSG_RECVMMSG_SIZE=16`)
  - TestMultipleSources: multiple clients → one server
  - Uses 30-second timeouts (UDP BPF event pipeline is slower than TCP)
  - **If `recvfrom` tests fail but `recvmsg` passes**: check `is_socket_failed()` handling in NetworkSignalHandler
- **TestConnectionsAndEndpointsUDPNormal**: UDP endpoint detection without send/recv tracking
- **TestCollectorStartup**: Basic smoke test — catches BPF loading failures immediately

### Diagnosing CI Failures

1. Check if the failure is a **BPF loading crash** (exit code 139, `scap_init` error) vs a **test logic failure**
2. Compare against master's CI run — if master passes on the same platform, it's a regression
3. Download log artifacts: `gh api repos/stackrox/collector/actions/artifacts/<id>/zip > logs.zip`
4. The `collector.log` file in the artifact contains full libbpf output including verifier errors
5. The test framework only shows the last few lines of collector logs in the CI output — always check the full artifact

## Step 9: Final Update

```sh
cd <collector_repo_root>
cd falcosecurity-libs && git checkout <final_version>-stackrox
cd .. && git add falcosecurity-libs
```

Update `docs/falco-update.md` with:
- Version transition (e.g., "0.23.1 → 0.25.0")
- Any new upstream API changes requiring collector-side fixes
- New StackRox patches added, patches dropped (upstreamed)
- Known issues or workarounds

## PR Strategy

Each stage should produce **two PRs**:
1. **Fork PR** targeting `upstream-main` in `stackrox/falcosecurity-libs` (the rebased branch)
2. **Collector PR** updating the submodule pointer and making collector-side code changes

## Quick Reference: Event Architecture

### How Process Events Flow

1. **Kernel BPF** captures syscall events → writes to ring buffer
2. **libscap** reads ring buffer → produces `scap_evt` structs
3. **libsinsp parsers** (`parsers.cpp`) process events:
   - `reset()`: looks up/creates thread info, validates enter/exit event matching
   - `parse_clone_exit_caller/child()`: creates child thread info, inherits parent fields
   - `parse_execve_exit()`: updates thread info with new process details
4. **Collector** (`ProcessSignalFormatter`) reads thread info fields via `EventExtractor`

### Key Thread Info Fields

| Field | Source | Notes |
|-------|--------|-------|
| `m_comm` | Exit event param 13 | Always correct (kernel task_struct->comm) |
| `m_exe` | Exit event param 1 | argv[0], may be relative |
| `m_exepath` | Enter event reconstruction OR param 27/30 | See "Exepath Resolution" gotcha |
| `m_pid` | Exit event param 4 | |
| `m_uid`/`m_gid` | Exit event param 26/29 | Was `m_user.uid()`/`m_group.gid()` before 0.20.0 |
| container_id | Extracted from thread cgroups via `GetContainerID()` | Was `m_container_id` before 0.21.0; plugin not used |

### Enter Event Deprecation

Upstream removed enter events to reduce ~50% of kernel/userspace overhead (proposal: `proposals/20240901-disable-support-for-syscall-enter-events.md`). All parameters moved to exit events. A scap converter handles old capture files. Any code depending on `retrieve_enter_event()` will silently fail with modern drivers — check for fallbacks using exit event parameters.

## Step 10: Update This Skill

**This step is mandatory.** After completing an update, review and update this skill file (`.claude/commands/update-falco-libs.md`) with anything learned during the process:

- **New API breakpoints**: Add entries to "Known historical API breakpoints" (Step 4) for any new breaking changes encountered
- **New StackRox patches**: Update the "Current StackRox Patches" list (Step 2) — add new patches, remove ones that were upstreamed
- **New gotchas**: Add to "Step 7: Known Gotchas" if you discovered new pitfalls (BPF verifier issues, parser bugs, build problems)
- **Outdated steps**: Remove or correct any steps that no longer apply (e.g., if an API listed as "changed in 0.22.0" is now the only way and doesn't need a migration note)
- **CI matrix updates**: Update the kernel compatibility matrix if CI platforms changed (new VM images, new kernel versions, platforms added/removed)
- **Fix patterns**: Add new "Common patterns of change" (Step 6) for any collector-side adaptations that future updates will likely need
- **Build/test changes**: Update build commands or test expectations if they changed

The goal is that the next person (or AI) performing an update has all the context from previous updates available, without needing to rediscover issues that were already solved.
