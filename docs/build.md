# Build System

## Overview

Multi-stage build combining CMake, Make, Docker, and vcpkg. Produces static C++ binary with CO-RE BPF probes. Supports amd64, arm64, ppc64le, s390x.

**Environment:** CentOS Stream 10 container (collector-builder)
**Output:** Static binary + CO-RE BPF probes
**Build time:** ~30-45 min (full), ~5 min (incremental)

## Build Flow

```
1. Builder Image (builder/Dockerfile)
   ├── Base: CentOS Stream 10
   ├── System packages: clang, cmake, gcc, etc.
   ├── Third-party deps from source:
   │   abseil, gperftools, protobuf, yaml-cpp
   │   grpc, civetweb, prometheus-cpp, jsoncpp
   │   tbb, libbpf, bpftool, valijson, uthash
   └── Workspace: /src

2. CMake Configuration (CMakeLists.txt)
   ├── Find packages (gRPC, yaml-cpp, civetweb, prometheus-cpp)
   ├── Compiler flags (C++17, -fPIC, -pthread)
   ├── Build types (Debug, Release)
   └── falcosecurity-libs (CO-RE BPF)

3. Collector Build (collector/Makefile)
   ├── cmake-configure: Generate build files
   ├── cmake-build: Compile C++ sources
   ├── Strip binary (Release)
   └── Copy to container/bin/

4. Container Image (collector/container/Dockerfile)
   ├── Base: UBI 10 Minimal
   ├── Runtime dependencies
   ├── Collector binary
   └── ENTRYPOINT: collector
```

## Builder Image

**Location:** `builder/Dockerfile`
**Base:** `quay.io/centos/centos:stream10`

System packages: clang, llvm, gcc, gcc-c++, cmake, make, autoconf, automake, libtool, glibc-devel, libcurl-devel, openssl-devel, binutils-devel, elfutils-libelf-devel, gdb, valgrind, libasan, libubsan, systemtap-sdt-devel.

Third-party dependencies built in `builder/install/` (execution order by prefix):
- 05: abseil (Google base library)
- 10: gperftools, protobuf, yaml-cpp
- 20: googletest
- 30: c-ares (async DNS)
- 35: re2 (regex)
- 40: civetweb (HTTP server), grpc
- 50: libb64, prometheus-cpp
- 60: jsoncpp, tbb
- 70: valijson, uthash
- 80: libbpf
- 90: bpftool

Versions in `builder/install/versions.sh`:
```bash
ABSEIL_VERSION="20240722.0"
GPERFTOOLS_VERSION="2.16"
PROTOBUF_VERSION="v28.3"
GRPC_VERSION="v1.67.0"
CIVETWEB_VERSION="v1.16"
```

Build debug builder (retains sources, debug symbols):
```bash
make builder BUILD_BUILDER_IMAGE=true COLLECTOR_BUILDER_DEBUG=true
```

## CMake Configuration

**Root:** `CMakeLists.txt` → `add_subdirectory(collector)`
**Collector:** `collector/CMakeLists.txt`

Package discovery:
```cmake
find_package(Threads)
find_package(CURL REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(civetweb CONFIG REQUIRED)
find_package(prometheus-cpp CONFIG REQUIRED)
```

Compiler flags:
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -fPIC -Wall --std=c++17 -pthread \
    -Wno-deprecated-declarations \
    -fno-omit-frame-pointer -rdynamic")
```

Build types:
- Debug: `-g -ggdb -D_DEBUG`
- Release: `-O3 -fno-strict-aliasing -DNDEBUG`

Sanitizers:
- ADDRESS_SANITIZER: `-fsanitize=address -fsanitize=undefined`
- THREAD_SANITIZER: `-fsanitize=thread`

Profiling: `DISABLE_PROFILING=OFF` enables `-DCOLLECTOR_PROFILING`.

Version injection:
```cmake
configure_file(
    lib/CollectorVersion.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/CollectorVersion.h)
```

## falcosecurity-libs Integration

```cmake
set(FALCO_DIR ${PROJECT_SOURCE_DIR}/../falcosecurity-libs)
add_subdirectory(${FALCO_DIR} falco)

set(BUILD_DRIVER OFF)  # No kernel module
set(USE_BUNDLED_DEPS OFF)  # System deps
set(BUILD_LIBSCAP_GVISOR OFF)  # No gVisor
set(SINSP_SLIM_THREADINFO ON)  # Optimize memory
set(BUILD_SHARED_LIBS OFF)  # Static linking
set(BUILD_LIBSCAP_MODERN_BPF ON)  # CO-RE BPF
set(MODERN_BPF_DEBUG_MODE ${BPF_DEBUG_MODE})
set(MODERN_BPF_EXCLUDE_PROGS "^(openat2|ppoll|setsockopt|io_uring_setup|nanosleep)$")
```

Definitions:
```cmake
add_definitions(-DSCAP_SOCKET_ONLY_FD)
add_definitions("-DINTERESTING_SUBSYS=\"perf_event\", \"cpu\", \"cpuset\", \"memory\"")
set(SCAP_HOST_ROOT_ENV_VAR_NAME "COLLECTOR_HOST_ROOT")
```

## Build Targets

**collector:** Main binary (links collector_lib)
**connscrape:** Connection scraping tool
**self-checks:** Startup validation
**collector_lib:** Core library from lib/

## Makefile System

**Root:** `Makefile` (includes `Makefile-constants.mk`)

Key targets:
- `tag`: Print git tag
- `builder`: Pull/build collector-builder
- `collector`: Build binary
- `image`: Build container image
- `image-dev`: Build with ubi (debugging)
- `unittest`: Run C++ unit tests
- `ci-integration-tests`: Run integration tests

Variables (`Makefile-constants.mk`):
```makefile
COLLECTOR_BUILDER_TAG ?= master
COLLECTOR_TAG ?= $(git describe --tags --abbrev=10 --dirty)
HOST_ARCH := $(uname -m | sed -e 's/x86_64/amd64/' -e 's/aarch64/arm64/')
PLATFORM ?= linux/$(HOST_ARCH)

CMAKE_BUILD_TYPE ?= Release
USE_VALGRIND ?= false
ADDRESS_SANITIZER ?= false
THREAD_SANITIZER ?= false
BPF_DEBUG_MODE ?= false
```

**Collector Makefile:** `collector/Makefile`

cmake-configure:
```makefile
cmake-configure/collector:
	docker exec $(COLLECTOR_BUILDER_NAME) \
		cmake -S $(BASE_PATH) -B $(CMAKE_DIR) \
			-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
			-DADDRESS_SANITIZER=$(ADDRESS_SANITIZER) \
			-DBPF_DEBUG_MODE=$(BPF_DEBUG_MODE) \
			-DCOLLECTOR_VERSION=$(COLLECTOR_VERSION)
```

cmake-build:
```makefile
cmake-build/collector: cmake-configure/collector
	docker exec $(COLLECTOR_BUILDER_NAME) \
		cmake --build $(CMAKE_DIR) -- -j $(NPROCS)
	docker exec $(COLLECTOR_BUILDER_NAME) \
		bash -c "[ $(CMAKE_BUILD_TYPE) == Release ] && \
		    strip --strip-unneeded $(COLLECTOR_BIN_DIR)/collector || exit 0"
```

Binary extraction:
```makefile
container/bin/collector: cmake-build/collector
	mkdir -p container/bin
	cp "$(COLLECTOR_BIN_DIR)/collector" container/bin/collector
	cp "$(COLLECTOR_BIN_DIR)/self-checks" container/bin/self-checks
```

## Container Image

**Dockerfile:** `collector/container/Dockerfile`

```dockerfile
FROM registry.access.redhat.com/ubi10/ubi-minimal:latest

ARG BUILD_TYPE=rhel  # or devel
ENV COLLECTOR_HOST_ROOT=/host

COPY container/${BUILD_TYPE}/install.sh /
RUN ./install.sh && rm -f install.sh

COPY container/THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY container/bin/collector /usr/local/bin/
COPY container/bin/self-checks /usr/local/bin/self-checks
COPY container/status-check.sh /usr/local/bin/status-check.sh

EXPOSE 8080 9090

HEALTHCHECK --start-period=5s --interval=5s \
    CMD /usr/local/bin/status-check.sh

ENTRYPOINT ["collector"]
```

Build types:
- **rhel:** Production (UBI 10 Minimal, stripped binary)
- **devel:** Development (UBI 10 full, debugging tools, unstripped)

Multi-arch:
```bash
PLATFORM=linux/amd64 make image
PLATFORM=linux/arm64 make image
PLATFORM=linux/ppc64le make image
PLATFORM=linux/s390x make image

docker buildx build --platform linux/amd64,linux/arm64 \
    -t quay.io/stackrox-io/collector:$(COLLECTOR_TAG) \
    collector/container/
```

## CO-RE BPF Probes

Built-in to binary (no external files). Generated during falcosecurity-libs build:

```cpp
// From bpf_probe.skel.h
static const unsigned char modern_bpf_probe[] = {
    0x7f, 0x45, 0x4c, 0x46, // ELF header
    // ... BPF bytecode
};
```

## Build Workflows

Local development:
```bash
make start-builder      # Start builder container
make collector          # Build binary
make image              # Build container
docker run --rm --privileged \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v /host:/host:ro \
    -e GRPC_SERVER=localhost:9999 \
    quay.io/stackrox-io/collector:$(COLLECTOR_TAG)
```

Incremental:
```bash
make collector  # ~2-5 min
make image      # ~30 sec
```

Clean:
```bash
make clean
make teardown-builder
make builder BUILD_BUILDER_IMAGE=true
make start-builder
make collector
```

CI multi-arch:
```bash
cd ansible
ansible-playbook ci-build-builder.yml \
    -e arch=amd64 \
    -e stackrox_io_username=$QUAY_USER \
    -e stackrox_io_password=$QUAY_PASS

ansible-playbook ci-build-collector.yml \
    -e arch=amd64 \
    -e collector_image=quay.io/stackrox-io/collector:$(COLLECTOR_TAG)
```

Cross-compilation (amd64 → arm64):
```bash
docker buildx create --use
docker buildx build --platform linux/arm64 \
    -t quay.io/stackrox-io/collector-builder:master-arm64 \
    -f builder/Dockerfile --load builder/

PLATFORM=linux/arm64 HOST_ARCH=arm64 make collector
```

## Performance

Timings (modern hardware):
| Task | Clean | Incremental |
|------|-------|-------------|
| Builder image | 30-45 min | N/A |
| CMake configure | 1-2 min | 5 sec |
| Collector compile | 8-12 min | 30 sec - 2 min |
| Container image | 1-2 min | 30 sec |

Parallelism:
```bash
make collector NPROCS=$(nproc)  # All cores
make collector NPROCS=4         # Limit (reduce memory)
```

Caching: Docker layer cache, CMake cache, incremental linking.

## Troubleshooting

Builder container:
```bash
docker ps -a | grep collector_builder  # Check exists
make teardown-builder                   # Restart
make start-builder
docker system prune -a                  # Disk space
```

CMake errors:
```bash
docker exec collector_builder_amd64 \
    cmake -S /src -B /src/cmake-build --debug-find

docker exec collector_builder_amd64 \
    cmake -S /src -B /src/cmake-build \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++
```

Compilation:
- Undefined reference: check link order in `collector/lib/CMakeLists.txt`, verify `target_link_libraries()`
- Header not found: verify `include_directories()`, check `git submodule update --init --recursive`

Binary issues:
```bash
make image-dev CMAKE_BUILD_TYPE=Debug  # Debug symbols
docker exec -it collector gdb /usr/local/bin/collector
file container/bin/collector            # Check stripped
ldd container/bin/collector             # Check libs
```

Container build:
```bash
docker pull registry.access.redhat.com/ubi10/ubi-minimal:latest
ls -la collector/container/bin/collector  # Verify exists
cat collector/container/.dockerignore     # Check ignore
```

## Advanced Options

Custom builder:
```bash
docker build -t my-collector-builder \
    --build-arg BASE_IMAGE=ubuntu:22.04 \
    -f builder/Dockerfile builder/
```

Custom falcosecurity-libs:
```bash
cd falcosecurity-libs
git remote add myfork https://github.com/myuser/falcosecurity-libs
git fetch myfork
git checkout myfork/my-feature
cd ..
make collector
```

Static analysis:
```bash
make -C collector check   # clang-format check
make -C collector format  # clang-format fix
docker exec collector_builder_amd64 \
    clang-tidy collector/lib/*.cpp -- -I...
```

Code coverage:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage"
cmake --build build
ctest --test-dir build
gcov build/collector/lib/*.o
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage-html
```

## Key Files

| File | Purpose |
|------|---------|
| Makefile | Root build orchestration |
| Makefile-constants.mk | Build variables |
| CMakeLists.txt | Root CMake |
| collector/Makefile | Collector build |
| collector/CMakeLists.txt | Collector CMake |
| builder/Dockerfile | Builder image |
| builder/install/*.sh | Dependency builds |
| builder/install/versions.sh | Version pinning |
| collector/container/Dockerfile | Runtime image |
| vcpkg.json | vcpkg manifest |

## References

- [CMake Documentation](https://cmake.org/documentation/)
- [gRPC C++ Build](https://grpc.io/docs/languages/cpp/quickstart/)
- [falcosecurity-libs](https://github.com/falcosecurity/libs)
- [Collector Architecture](architecture.md)
- [Integration Tests](integration-tests.md)
