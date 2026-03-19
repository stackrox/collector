# Collector

C++ eBPF runtime security agent. Captures process, network, and container
events via CO-RE BPF (falcosecurity-libs) and reports to StackRox Sensor
via gRPC.

## Build (inside devcontainer)

```bash
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release \
  -DCOLLECTOR_VERSION=$(git describe --tags --abbrev=10 --long)
cmake --build cmake-build -- -j$(nproc)
ctest --no-tests=error -V --test-dir cmake-build
```

## Key Paths

```
collector/lib/          C++ core library (~108 files)
collector/test/         Unit tests (GTest/GMock, 17 suites)
collector/collector.cpp Main entrypoint
falcosecurity-libs/     Submodule: eBPF engine + CO-RE BPF programs
integration-tests/      Go test framework (26 suites, needs privileged)
```

## Testing Rules

- Unit tests validate C++ logic only — no kernel needed
- eBPF changes CANNOT be tested locally — push PR, CI runs on real kernels
- CI matrix: rhel, ubuntu, cos, flatcar, fedora-coreos (amd64/arm64/s390x/ppc64le)

## Conventions

- C++17, clang, `clang-format --style=file`
- Do not push to remote without explicit permission
