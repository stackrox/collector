---
name: build
description: Build collector binary with options (debug, asan, tsan, clean)
tags: [collector, build, cmake, cpp]
---

# Build Collector

Build the collector binary. Supports optional arguments:
- `debug` — Debug build with symbols
- `asan` — AddressSanitizer build
- `tsan` — ThreadSanitizer build
- `clean` — Clean build directory first
- (no args) — Release build

## Steps

1. Determine build environment:
   - If inside the devcontainer (check: `/usr/local/bin/cmake` exists and we're on Linux), run cmake directly.
   - If on the host (macOS), use `make start-builder && make collector`.

2. If `clean` argument is provided, remove `cmake-build/` directory first.

3. Set build variables based on arguments:
   - `debug`: `CMAKE_BUILD_TYPE=Debug`
   - `asan`: `CMAKE_BUILD_TYPE=Debug`, `ADDRESS_SANITIZER=ON`
   - `tsan`: `CMAKE_BUILD_TYPE=Debug`, `THREAD_SANITIZER=ON`
   - default: `CMAKE_BUILD_TYPE=Release`

4. Run cmake configure (if `cmake-build/` doesn't exist or CMakeLists.txt changed):
   ```bash
   cmake -S . -B cmake-build \
     -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
     -DADDRESS_SANITIZER=$ADDRESS_SANITIZER \
     -DTHREAD_SANITIZER=$THREAD_SANITIZER \
     -DCOLLECTOR_VERSION=$(git describe --tags --abbrev=10 --long)
   ```

5. Run cmake build:
   ```bash
   cmake --build cmake-build -- -j$(nproc)
   ```

6. Report result: success with binary size, or failure with the first error and its file:line.
