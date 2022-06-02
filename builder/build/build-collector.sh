#!/usr/bin/env bash
set -eux

ldconfig -v

SRC_ROOT_DIR="${SRC_ROOT_DIR:-/tmp/collector}"
CMAKE_BUILD_DIR="${CMAKE_BUILD_DIR:-${SRC_ROOT_DIR}/cmake-build}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
ADDRESS_SANITIZER="${ADDRESS_SANITIZER:-false}"
COLLECTOR_APPEND_CID="${COLLECTOR_APPEND_CID:-false}"

if [ "$ADDRESS_SANITIZER" = "true" ]; then
    # Needed for address sanitizer to work. See https://github.com/grpc/grpc/issues/22238.
    # When Collector is built with address sanitizer it sets GRPC_ASAN_ENABLED, which changes a struct in the grpc library.
    # If grpc is compiled without that flag and is then linked with Collector the struct will have
    # two different definitions and Collector will crash when trying to connect to a grpc server.
    while read -r file; do
        sed -i 's|#include <grpc/impl/codegen/port_platform.h>|#include <grpc/impl/codegen/port_platform.h>\n#ifdef GRPC_ASAN_ENABLED\n#  undef GRPC_ASAN_ENABLED\n#endif|' "$file"
    done < <(grep -rl port_platform.h "${SRC_ROOT_DIR}/generated" --include=*.h)
fi

cmake_extra_flags=(
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
    -DADDRESS_SANITIZER="$ADDRESS_SANITIZER"
    -DCOLLECTOR_APPEND_CID="$COLLECTOR_APPEND_CID"
)

cmake "${cmake_extra_flags[@]}" -S "${SRC_ROOT_DIR}" -B "${CMAKE_BUILD_DIR}"
cmake --build "${CMAKE_BUILD_DIR}" --target all -- -j "${NPROCS:-2}"

if [ "$CMAKE_BUILD_TYPE" = "Release" ]; then
    strip --strip-unneeded \
        "${CMAKE_BUILD_DIR}/collector/collector" \
        "${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so"
fi

cp -r /THIRD_PARTY_NOTICES "${CMAKE_BUILD_DIR}/"
