#!/usr/bin/env bash
set -e

export PATH="/usr/local/go/bin:${PATH}"

cd third_party/falcosecurity-plugins/plugins/container

cp ../../LICENSE "${LICENSE_DIR}/falcosecurity-plugins-container-${CONTAINER_PLUGIN_VERSION}" 2> /dev/null || true

# Remove static libstdc++ linking — not needed since we control the runtime
# image, and libstdc++-static is not available in CentOS Stream 10.
sed -i '/-static-libgcc\|-static-libstdc++/d' CMakeLists.txt

cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_ASYNC=ON \
    -DENABLE_TESTS=OFF
cmake --build build --target container --parallel "${NPROCS}"

install -m 755 build/libcontainer.so /usr/local/lib64/libcontainer.so
