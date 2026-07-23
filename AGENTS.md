# Collector Agent Context

Collector is a C++ eBPF runtime-security agent. It uses CO-RE BPF via
falcosecurity-libs to report process, network, and container events to
StackRox Sensor over gRPC.

## Repository layout

```
collector/lib/        C++ implementation
collector/test/       GTest unit tests
falcosecurity-libs/   vendored probe and sinsp library (submodule)
integration-tests/    Go/testify integration suites (need Docker + privileged)
.github/workflows/    CI and agent workflows
```

## Build (inside builder container)

```bash
make start-builder       # start builder container
make collector           # cmake configure + build
make unittest            # ctest inside builder
```

Or directly:

```bash
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release \
  -DCOLLECTOR_VERSION=$(git describe --tags --abbrev=10 --long)
cmake --build cmake-build -- -j$(nproc)
ctest --no-tests=error -V --test-dir cmake-build
```

## Format

```bash
clang-format --style=file -i <changed .cpp/.h files>
```

## Unit tests

Unit tests validate C++ logic only. They do not require a live kernel,
eBPF, or Docker. They run inside the builder container via `ctest`.

## Integration tests

Integration tests live in `integration-tests/` and require Docker with
privileged mode. They exercise eBPF probe loading and real kernel
interaction across a matrix of OS and architecture combinations. These
run in CI only.

## Exclusions

Agent-generated changes must NOT touch:

- eBPF probes, BTF, falcosecurity-libs, or kernel probe loading
- capabilities, privileged execution, or security contexts
- Collector/Sensor gRPC protocol or event semantics
- lifecycle, threading, reconnection, or backpressure logic
- build images, base image dependencies, or release infrastructure
- workflow infrastructure, secrets, or CI matrix configuration

Return `blocked` immediately if a task reaches any of these areas.

## Review guidance

### C++
- RAII for all resource management; no raw new/delete
- Check ownership and lifetime at every pointer handoff
- Thread safety: document locking order, no lock-free cleverness
- Error paths must release resources

### eBPF (if reviewing, never generating)
- Verifier constraints are absolute
- Userspace ABI changes require paired kernel/userspace updates

### Go integration tests
- Deterministic waits (poll with timeout), not fixed sleeps
- Always clean up containers and resources
- Useful failure messages with actual vs expected

### Shell
- `set -euo pipefail` in every script
- Quote all variable expansions
- Propagate exit codes

### GitHub Actions
- Pin every action to a full commit SHA
- Minimum token permissions for each job
- Never evaluate untrusted input as shell
