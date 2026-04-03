# Documentation Map: `doc/` vs `docs/`

> **Status: MERGE COMPLETE.** All content from `doc/` has been copied into `docs/` with inaccuracies fixed. The `doc/` directory can be removed in a follow-up PR. The top-level `README.md` has been updated with links to all new documentation.

## Overview (pre-merge analysis)

The repository had **two** documentation directories with different origins and purposes:

| Directory | Files | Origin | Style |
|-----------|-------|--------|-------|
| `docs/`   | 8 files + 1 image | Original, human-written | Concise, operational guides |
| `doc/`    | 16 files (6 top-level + 9 in `lib/` + 1 verification report) | Newer, AI-assisted deep dives | Comprehensive architecture docs |

The top-level `README.md` only links to `docs/` files. The `doc/` directory is currently **unlinked** from any navigation.

---

## File Inventory

### `docs/` (existing, linked from README.md)

| File | Purpose | Length | Status |
|------|---------|--------|--------|
| `how-to-start.md` | Dev setup, building, debugging, hotreload | ~281 lines | Mostly current; some Docker Desktop bug refs may be stale |
| `design-overview.md` | High-level architecture stub | ~31 lines | **Incomplete** — several empty sections |
| `troubleshooting.md` | Log retrieval, startup errors, metrics, profiling, introspection | ~597 lines | Current; some profiler paths may be stale |
| `references.md` | Env vars, CLI args, JSON config, runtime config | ~191 lines | Current |
| `release.md` | Automated + manual release process | ~138 lines | tag-bumper.py noted as out of date |
| `falco-update.md` | How to rebase the falcosecurity-libs fork | ~72 lines | Current |
| `driver-builds.md` | CPaaS/OSCI driver pipeline diagrams | ~57 lines | Current (diagram-heavy) |
| `labels.md` | GHA labels for CI control | ~8 lines | Current |

### `doc/` (new, NOT linked from README.md)

| File | Purpose | Length | Status |
|------|---------|--------|--------|
| `README.md` | Full architecture overview: components, data flow, threading, config, testing, deployment, performance | ~800 lines | Has known inaccuracies (see below) |
| `lib.md` | Deep dive into collector/lib C++ codebase (~108 files) | ~70KB | Has hardcoded paths, stale line counts |
| `falcosecurity-libs.md` | BPF driver integration: layers, syscalls, ring buffers, fork patches | ~150 lines | Version 3.21.0-2 should be 3.21.6 |
| `integration-tests.md` | Test framework: 26 suites (doc says 27), mock sensor, CI | ~310 lines | Test count off by 1 |
| `build.md` | Multi-stage CMake/Docker build: deps, targets, workflows | ~420 lines | gRPC/Civetweb/Protobuf versions wrong |
| `deployment.md` | Ansible automation, VM lifecycle, K8s DaemonSet, CI | ~610 lines | Current |
| `ebpf-architecture.md` | CO-RE BPF deep dive: tracepoints, tail calls, ring buffers, verifier | ~1200 lines | Current |
| `VERIFICATION_REPORT.md` | QA report on doc accuracy vs source code | ~440 lines | Meta-document, dated 2026-03-13 |
| `lib/README.md` | Index of C++ library components | ~180 lines | Current |
| `lib/grpc.md` | gRPC bidirectional streaming layer | ~150 lines | Current |
| `lib/config.md` | Config system: args, env vars, YAML, hot reload | ~190 lines | Current |
| `lib/core.md` | Main loop, lifecycle, metrics, health endpoints | ~150 lines | Current |
| `lib/containers.md` | Container ID extraction, metadata caching | ~140 lines | Current |
| `lib/system.md` | SystemInspector ↔ BPF abstraction boundary | ~230 lines | Current |
| `lib/networking.md` | Network flow tracking, ConnTracker, Afterglow | ~130 lines | Current |
| `lib/process.md` | Process exec events, lineage tracking | ~70 lines | Current |

---

## Topic Overlap Analysis

| Topic | `docs/` file | `doc/` file | Overlap Level | Notes |
|-------|-------------|------------|---------------|-------|
| Architecture overview | `design-overview.md` (stub) | `README.md` (comprehensive) | **Replace** | `doc/README.md` is a full replacement for the incomplete `docs/design-overview.md` |
| Falcosecurity-libs | `falco-update.md` (how to rebase) | `falcosecurity-libs.md` (architecture) | **Complementary** | Different angles: one is process, the other is architecture |
| Build system | `how-to-start.md` (dev setup) | `build.md` (build internals) | **Complementary** | `how-to-start.md` = getting started; `build.md` = deep dive |
| Testing | — | `integration-tests.md` | **New coverage** | No equivalent in `docs/` |
| Deployment/Ansible | — | `deployment.md` | **New coverage** | No equivalent in `docs/` |
| eBPF internals | — | `ebpf-architecture.md` | **New coverage** | No equivalent in `docs/` |
| C++ library internals | — | `lib/*.md` (8 files) | **New coverage** | No equivalent in `docs/` |
| Troubleshooting | `troubleshooting.md` | (partial in `README.md`) | **Keep docs/** | `docs/troubleshooting.md` is the authoritative source |
| Config reference | `references.md` | `lib/config.md` | **Complementary** | `references.md` = user-facing ref; `lib/config.md` = implementation |
| Release process | `release.md` | — | **Keep docs/** | Only in `docs/` |
| CI labels | `labels.md` | — | **Keep docs/** | Only in `docs/` |
| Driver builds | `driver-builds.md` | — | **Keep docs/** | Only in `docs/` |

---

## Known Inaccuracies to Fix

These were identified in `doc/VERIFICATION_REPORT.md` (2026-03-13):

| File | Issue | Doc Says | Actual |
|------|-------|----------|--------|
| `doc/README.md`, `doc/integration-tests.md` | Test suite count | 27 | 26 |
| `doc/README.md`, `doc/lib.md` | C++ line count | ~15,778 | ~16,521 |
| `doc/lib.md` | Class name | ConnectionTracker | ConnTracker |
| `doc/build.md` | gRPC version | v1.68.3 | v1.67.0 |
| `doc/build.md` | Civetweb version | v1.17 | v1.16 |
| `doc/build.md` | Protobuf version | v29.3 | v28.3 |
| `doc/falcosecurity-libs.md` | Libs version | 3.21.0-2 | 3.21.6 |
| `doc/lib.md` | Hardcoded path | `/Users/rc/go/src/...` | Should be relative |

---

## Recommended Merge Strategy

**Goal:** Unify under `docs/` without changing existing files, by adding new files and updating only the top-level `README.md` links.

### Step 1: Move `doc/` content into `docs/`

Add the following new files to `docs/` (no existing files modified):

| New `docs/` file | Source | Action |
|-------------------|--------|--------|
| `docs/architecture.md` | `doc/README.md` | Copy, fix inaccuracies, replace `docs/design-overview.md` link in README |
| `docs/integration-tests.md` | `doc/integration-tests.md` | Copy, fix test count |
| `docs/build.md` | `doc/build.md` | Copy, fix dependency versions |
| `docs/deployment.md` | `doc/deployment.md` | Copy as-is |
| `docs/ebpf-architecture.md` | `doc/ebpf-architecture.md` | Copy as-is |
| `docs/falcosecurity-libs.md` | `doc/falcosecurity-libs.md` | Copy, fix version, keep alongside `falco-update.md` |
| `docs/lib/` (entire dir) | `doc/lib/` | Copy directory, fix ConnTracker class name |

### Step 2: Update top-level `README.md`

Add links for the new documentation (minimal change):

```markdown
## Useful links

1. [How to start](docs/how-to-start.md): Building and contributing.
2. [Architecture](docs/architecture.md): How Collector works internally.
3. [Troubleshooting](docs/troubleshooting.md): Common errors and diagnostics.
4. [Release Process](docs/release.md): Release procedures.
5. [References](docs/references.md): Configuration options.

## Deep Dives

6. [eBPF Architecture](docs/ebpf-architecture.md): CO-RE BPF kernel instrumentation.
7. [Build System](docs/build.md): CMake/Docker build pipeline.
8. [Integration Tests](docs/integration-tests.md): Test framework and suites.
9. [Deployment](docs/deployment.md): Ansible automation and K8s deployment.
10. [Falcosecurity-libs](docs/falcosecurity-libs.md): BPF driver architecture.
11. [Falco Fork Update](docs/falco-update.md): How to rebase the fork.
12. [C++ Library Internals](docs/lib/README.md): Code-level documentation.
13. [Driver Builds](docs/driver-builds.md): CPaaS/OSCI pipeline.
14. [CI Labels](docs/labels.md): GitHub Actions labels.
```

### Step 3: Retire `doc/`

After merging, `doc/` can be:
- Kept as-is for reference (no links point to it)
- Or deleted in a follow-up PR

### Step 4: Deprecate `docs/design-overview.md`

Add a note at the top redirecting to `docs/architecture.md`:
```markdown
> **Note:** This document has been superseded by [Architecture](architecture.md).
```

### What does NOT need to change

- `docs/how-to-start.md` — keep as-is
- `docs/troubleshooting.md` — keep as-is
- `docs/references.md` — keep as-is
- `docs/release.md` — keep as-is
- `docs/labels.md` — keep as-is
- `docs/driver-builds.md` — keep as-is
- `docs/falco-update.md` — keep as-is

### Files to skip during merge

- `doc/lib.md` — superseded by `doc/lib/README.md` + individual `doc/lib/*.md` files; too large and has hardcoded paths
- `doc/VERIFICATION_REPORT.md` — meta-document, not user-facing; use it to fix inaccuracies then discard

---

## Summary

The `doc/` directory contains high-quality architecture documentation that **fills major gaps** in `docs/` (no testing, build, deployment, eBPF, or library internals docs existed before). The only real overlap is `design-overview.md` which is incomplete and should be superseded. The merge is low-risk: add new files to `docs/`, fix known inaccuracies, update the README links.
