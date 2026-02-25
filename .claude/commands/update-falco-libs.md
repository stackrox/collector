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

# Breaking changes
git log --oneline <current>..<target> --grep="BREAKING\|breaking\|!:"
```

Then grep the collector code for uses of changed/removed APIs:

```sh
grep -rn '<removed_api_name>' collector/lib/ collector/test/ --include='*.cpp' --include='*.h'
```

Key collector integration points to check:
- `collector/lib/system-inspector/Service.cpp` — sinsp initialization, container setup, thread access
- `collector/lib/system-inspector/EventExtractor.h` — threadinfo field access macros (TINFO_FIELD, TINFO_FIELD_RAW_GETTER)
- `collector/lib/ContainerMetadata.cpp` — container info/label lookup
- `collector/lib/ContainerEngine.h` — container engine integration (may be deleted if upstream removed container engines)
- `collector/lib/ProcessSignalFormatter.cpp` — process signal creation, container_id access, parent thread traversal
- `collector/lib/Process.cpp` — process info access, container_id
- `collector/lib/Utility.cpp` — threadinfo printing
- `collector/test/ProcessSignalFormatterTest.cpp` — thread creation, get_thread_ref, container_id setup
- `collector/CMakeLists.txt` — falco build flags (SINSP_SLIM_THREADINFO, BUILD_LIBSCAP_MODERN_BPF, MODERN_BPF_EXCLUDE_PROGS, etc.)

## Step 4: Plan Staging Strategy

If the gap is large (>200 commits), identify intermediate stopping points:

1. Look for version boundaries where major API changes happen
2. Prefer stopping at versions where container/thread APIs change
3. Each stage should be independently buildable and testable

Known historical API breakpoints (update as upstream evolves):
- **0.20.0**: `set_import_users` lost second arg, user/group structs on threadinfo replaced with `m_uid`/`m_gid`
- **0.21.0**: Container engine subsystem removed entirely, replaced by container plugin (`libcontainer.so`). `m_container_id` removed from threadinfo. `m_thread_manager` changed to `shared_ptr`. `build_threadinfo()`/`add_thread()` removed from sinsp.
- **0.22.0**: `get_thread_ref` removed from sinsp (use `find_thread`)
- **0.23.0+**: `get_container_id()` removed from threadinfo. Parent thread traversal moved to thread_manager. User/group handling removed from threadinfo.

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

Common patterns of change:
- **Removed container engine**: Delete `ContainerEngine.h`, replace `set_container_engine_mask()` with `register_plugin()` for the container plugin
- **Container plugin**: Ship `libcontainer.so` in the container image, register it before setting filters like `container.id != 'host'`
- **Container metadata**: Replace `m_container_manager.get_containers()` with plugin state table API
- **Thread access**: Replace `get_thread_ref(tid, true)` with `find_thread(tid, false)` or `m_thread_manager->find_thread()`
- **Container ID**: Replace `tinfo->m_container_id` with `tinfo->get_container_id()` or `FIELD_CSTR(container_id, "container.id")`
- **User/group**: Replace `m_user.uid()` / `m_group.gid()` with `m_uid` / `m_gid`
- **Thread creation in tests**: Replace `build_threadinfo()` with `m_thread_manager_factory.create()`, `add_thread()` with `m_thread_manager->add_thread()`

## Step 7: Validate Each Stage

Run this checklist after each stage:

- [ ] `falcosecurity-libs` builds via cmake `add_subdirectory`
- [ ] Each surviving patch verified: diff against original to ensure no content loss
- [ ] `make collector` succeeds on amd64
- [ ] `make unittest` passes (all test suites, especially ProcessSignalFormatterTest)
- [ ] Multi-arch compilation: arm64, ppc64le, s390x
- [ ] Integration tests on at least 1 VM type (RHCOS or Ubuntu)
- [ ] Runtime self-checks pass
- [ ] Container ID attribution works correctly
- [ ] Container label/namespace lookup works
- [ ] Network signal handler receives correct container IDs
- [ ] No ASan/Valgrind errors (run with `ADDRESS_SANITIZER=ON` and `USE_VALGRIND=ON`)
- [ ] Performance benchmarks show no regression vs previous stage

## Step 8: Final Update

```sh
cd <collector_repo_root>
cd falcosecurity-libs && git checkout <final_version>-stackrox
cd .. && git add falcosecurity-libs
```

Update `docs/falco-update.md` with notes about what changed.

## PR Strategy

Each stage should produce **two PRs**:
1. **Fork PR** targeting `upstream-main` in `stackrox/falcosecurity-libs` (the rebased branch)
2. **Collector PR** updating the submodule pointer and making collector-side code changes

## Important Notes

- The container plugin (`libcontainer.so`) replaced built-in container engines at upstream 0.21.0
- Container plugin source: `https://github.com/falcosecurity/plugins/tree/main/plugins/container`
- Container plugin is C++/Go hybrid — needs Go toolchain to build from source
- Upstream only ships x86_64 and arm64 binaries; ppc64le/s390x must be built from source
- The `giles/cherry-picked-stackrox-additions` branch may have useful reference patches for collector-side adaptations
- Previous update attempts (e.g., `0.21.0-stackrox-rc1`) can be used as conflict resolution references
