# Collector - Agent Development Guide

Collector is a C++ eBPF-based runtime security agent that captures process,
network, and container events from Linux kernels. It uses CO-RE BPF
(Compile Once, Run Everywhere) via falcosecurity-libs and reports events
to StackRox Sensor via gRPC.

## Quick Reference

```bash
# Build (uses builder container with all C++ deps)
make start-builder          # Start builder container (first time / after reboot)
make collector              # Compile collector binary (~30s incremental)
make image                  # Build container image
make image-dev              # Build dev image (with package manager, gdb)

# Test
make unittest               # C++ unit tests via ctest (~1 min)
CMAKE_BUILD_TYPE=Debug make unittest  # With debug symbols

# Lint
make check-clang-format-all # Check C++ formatting
make check-flake8-all       # Check Python

# Integration tests (local, requires Docker + privileged)
cd integration-tests
make TestProcessNetwork     # Single test suite
make ci-integration-tests   # Full suite (2h timeout)
```

## Architecture

```
collector/                    # Main C++ application
├── lib/                      # Core library (~108 files)
│   ├── KernelDriver.h        # eBPF probe lifecycle (Setup/Start/Stop)
│   ├── CollectorService.cpp  # Main service loop
│   ├── ConnTracker.cpp       # Connection state machine
│   ├── NetworkConnection.h   # IP/port/protocol structures
│   └── ProcessSignalHandler.h # Process event formatting
├── test/                     # Unit tests (GTest/GMock)
├── container/Dockerfile      # Production container (UBI minimal)
└── collector.cpp             # Main entrypoint

falcosecurity-libs/           # Submodule: eBPF engine
└── driver/modern_bpf/        # CO-RE BPF programs
    ├── programs/attached/    # Tracepoint handlers (sys_enter, sys_exit, sched_*)
    └── maps/                 # BPF maps (ring buffers, tail call tables)

builder/                      # Builder image (CentOS Stream 10)
├── Dockerfile
└── install/                  # Dependency build scripts (grpc, protobuf, libbpf, etc.)

integration-tests/            # Go test framework (testify/suite)
├── suites/                   # 26 test suites
├── pkg/mock_sensor/          # Mock gRPC sensor
└── pkg/executor/             # Container runtime abstraction

ansible/                      # VM lifecycle and test orchestration
├── integration-tests.yml     # Create VM → provision → test → destroy
├── dev/                      # Developer inventory (acs-team-sandbox)
└── roles/                    # create-vm, provision-vm, run-test-target
```

## Development Workflow

### For C++ / library changes (non-eBPF)

Changes to `collector/lib/` that don't touch kernel interaction:

1. Edit source files
2. `make collector` — compile (~30s incremental)
3. `make unittest` — run unit tests
4. Push PR — CI validates across platforms

Unit tests cover: config parsing, connection tracking, network structures,
process filtering, event formatting, host info detection.

### For eBPF / kernel driver changes

Changes to `falcosecurity-libs/driver/modern_bpf/` or `collector/lib/KernelDriver.h`:

1. Edit source files
2. `make collector` — compile (eBPF compiles to skeleton header)
3. `make unittest` — validates C++ logic only
4. **Push PR** — CI runs integration tests on real kernels
5. Monitor CI: `.github/workflows/integration-tests.yml` runs on
   rhel, ubuntu, cos, flatcar, fedora-coreos across amd64/arm64/s390x/ppc64le

**Unit tests CANNOT validate eBPF changes.** The BPF programs must load into
a real kernel with BTF support. CI handles this across 10+ VM types.

### For integration test changes

Changes to `integration-tests/`:

1. Edit Go test files
2. Build test binary: `cd integration-tests && make build`
3. Run locally if Docker available: `make TestProcessNetwork`
4. Push PR — CI runs full matrix

### Build Variables

| Variable | Default | Purpose |
|----------|---------|---------|
| CMAKE_BUILD_TYPE | Release | Release or Debug |
| ADDRESS_SANITIZER | OFF | Enable AddressSanitizer |
| THREAD_SANITIZER | OFF | Enable ThreadSanitizer |
| USE_VALGRIND | OFF | Valgrind profiling |
| BPF_DEBUG_MODE | OFF | BPF debug output |
| COLLECTOR_BUILDER_TAG | master | Builder image version |

### Running collector locally

```yaml
# docker-compose.dev.yml pattern:
services:
  collector:
    image: quay.io/stackrox-io/collector:${COLLECTOR_TAG}
    privileged: true
    network_mode: host
    environment:
      - GRPC_SERVER=localhost:9999
      - COLLECTION_METHOD=core-bpf
      - COLLECTOR_HOST_ROOT=/host
    volumes:
      - /var/run/docker.sock:/host/var/run/docker.sock:ro
      - /proc:/host/proc:ro
      - /etc:/host/etc:ro
      - /sys/:/host/sys/:ro
```

Standalone mode (no gRPC server, outputs JSON to stdout):
```bash
collector --grpc-server=
```

### Hotreload on local K8s

For rapid iteration without rebuilding the container image:
```bash
# Deploy stackrox to a local cluster first, then:
./utilities/hotreload.sh
# Recompile with: make -C collector container/bin/collector
```

## Key Dependencies

- gRPC v1.67.0, Protobuf v28.3
- libbpf v1.3.4, CO-RE BPF (kernel >= 5.8 with BTF)
- falcosecurity-libs (submodule, scap + sinsp)
- Google Test v1.15.2

## CI Pipeline

Push to PR triggers `.github/workflows/main.yml`:
1. **init** — set tags, determine what to build
2. **build-collector** — multi-arch compile
3. **unit-tests** — ctest (Release, ASAN, Valgrind)
4. **integration-tests** — VM matrix (rhel, ubuntu, cos, flatcar, etc.)
5. **k8s-integration-tests** — KinD cluster tests
6. **benchmarks** — performance (master only or `run-benchmark` label)

### Triggering specific CI behavior

- Add `build-builder-image` label to rebuild the builder
- Add `run-benchmark` label for performance tests
- Add `update-baseline` label to update benchmark baseline

## File Conventions

- C++17, compiled with clang
- Format: `clang-format` (check with `make check-clang-format-all`)
- Integration tests: Go with testify/suite
- Shell scripts: `shfmt` + `shellcheck`
- Python: `flake8`
- Git hooks: `pre-commit` (run `make init-githook`)
