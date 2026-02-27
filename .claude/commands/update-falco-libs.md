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
# Container engine / plugin changes
git log --oneline <current>..<target> -- userspace/libsinsp/container_engine/
git log --oneline <current>..<target> --grep="container_engine\|container plugin"

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
- `collector/lib/system-inspector/Service.cpp` — sinsp initialization, plugin loading, filter setup
- `collector/lib/system-inspector/EventExtractor.h` — threadinfo field access macros (TINFO_FIELD, FIELD_CSTR, FIELD_RAW)
- `collector/lib/ContainerMetadata.cpp` — container info/label lookup
- `collector/lib/ProcessSignalFormatter.cpp` — process signal creation, exepath access, container_id, lineage traversal
- `collector/lib/NetworkSignalHandler.cpp` — container_id access
- `collector/lib/Process.cpp` — process info access, container_id
- `collector/lib/Utility.cpp` — GetContainerID helper, threadinfo printing
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
- **0.21.0**: Container engine subsystem removed entirely, replaced by container plugin (`libcontainer.so`). `m_container_id` removed from threadinfo. `m_thread_manager` changed to `shared_ptr`. `build_threadinfo()`/`add_thread()` removed from sinsp. Enter events for many syscalls deprecated (`EF_OLD_VERSION`).
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

**Container plugin integration** (from 0.21.0):
- Delete `ContainerEngine.h` — container engines no longer built-in
- Ship `libcontainer.so` in the collector image (built from source in builder, needs Go)
- Load via `sinsp::register_plugin()` in Service.cpp before setting filters
- Register extraction capabilities: `EventExtractor::FilterList().add_filter_check(sinsp_plugin::new_filtercheck(plugin))`
- Wrap `set_filter("container.id != 'host'")` in try-catch for tests without plugin

**Container ID access** (from 0.21.0+):
- Replace `tinfo->m_container_id` with a helper like `GetContainerID(tinfo, thread_manager)` that reads from plugin state tables
- In EventExtractor.h: change `TINFO_FIELD(container_id)` to `FIELD_CSTR(container_id, "container.id")` (provided by container plugin)
- The `FIELD_CSTR` null guard handles tests where the plugin isn't loaded

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

### Container Plugin Build

The container plugin (`libcontainer.so`) is a C++/Go hybrid:
- Source: `github.com/falcosecurity/plugins` (monorepo), `plugins/container/` directory
- Requires Go 1.23+ for the go-worker component
- Upstream only ships x86_64 and arm64 binaries; ppc64le/s390x must be built from source
- Version must match falcosecurity-libs (check plugin compatibility matrix)
- Submodule at `builder/third_party/falcosecurity-plugins`

### BPF Verifier Compatibility

BPF verifier behavior varies significantly across:
- **Kernel versions**: Older kernels have stricter limits
- **Clang versions**: clang > 19 can produce code that exceeds instruction counts
- **Platform kernels**: RHEL SAP, Google COS have custom verifiers

Common fixes:
- Reduce loop bounds (e.g., `MAX_IOVCNT` 32 → 16)
- Mark variables `volatile` to prevent optimizations the verifier can't follow
- Add `#pragma unroll` for loops the verifier can't bound
- Break pointer chain reads into separate variables with null checks
- Use `const` qualifiers on credential struct pointers

## Step 8: Validate Each Stage

Run this checklist after each stage:

- [ ] `falcosecurity-libs` builds via cmake `add_subdirectory`
- [ ] Each surviving patch verified: diff against original to ensure no content loss
- [ ] `make collector` succeeds on amd64
- [ ] `make unittest` passes (all test suites, especially ProcessSignalFormatterTest)
- [ ] Integration tests: `TestProcessViz` (exepath correctness), `TestProcessLineageInfo`, `TestNetworkFlows`
- [ ] Multi-arch compilation: arm64, ppc64le, s390x
- [ ] Container ID attribution works (not all showing empty or host)
- [ ] Process exepaths are correct (not showing container runtime binary like `/usr/bin/podman`)
- [ ] Container label/namespace lookup works
- [ ] Network signal handler receives correct container IDs
- [ ] Runtime self-checks pass

### Key Integration Tests

- **TestProcessViz**: Verifies process ExePath, Name, and Args for container processes. Catches the exepath regression where all paths show the container runtime. Expected paths like `/bin/ls`, `/usr/sbin/nginx`, `/bin/sh`.
- **TestProcessLineageInfo**: Verifies parent process lineage chains stop at container boundaries.
- **TestNetworkFlows**: Verifies network connections are attributed to correct containers.

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
| container_id | Container plugin filter field | Was `m_container_id` before 0.21.0 |

### Enter Event Deprecation

Upstream removed enter events to reduce ~50% of kernel/userspace overhead (proposal: `proposals/20240901-disable-support-for-syscall-enter-events.md`). All parameters moved to exit events. A scap converter handles old capture files. Any code depending on `retrieve_enter_event()` will silently fail with modern drivers — check for fallbacks using exit event parameters.
