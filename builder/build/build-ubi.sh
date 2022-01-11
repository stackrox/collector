#!/usr/bin/env bash

# This builds the collector and its dependencies with the assumption that a Red
# Hat subscription provides access to required RHEL 8 RPMs and that dependencies
# are resolved with submodules.

set -eux

source ./builder/install/versions.sh

export WITH_RHEL8_RPMS="true"
export LICENSE_DIR="/THIRD_PARTY_NOTICES"
mkdir -p "${LICENSE_DIR}"

### Generate source from protobufs

cd third_party
../builder/install/40-grpc.sh
cd ..

ln -s third_party/googleapis googleapis

BASE_PATH=$(pwd) make -f builder/build/protogen.mk SKIP_GOOGLEAPI_FETCH=1 generated-proto-srcs

### Dependencies

cd third_party
../builder/install/50-libb64.sh
../builder/install/50-luajit.sh
../builder/install/50-prometheus.sh .
cd ..

### Build

export DISABLE_PROFILING="true"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
ADDRESS_SANITIZER="${ADDRESS_SANITIZER:-false}"

cp -a collector/generated src/generated

if [ $ADDRESS_SANITIZER = "true" ]; then
    # Needed for address sanitizer to work. See https://github.com/grpc/grpc/issues/22238.
    # When Collector is built with address sanitizer it sets GRPC_ASAN_ENABLED, which changes a struct in the grpc library.
    # If grpc is compiled without that flag and is then linked with Collector the struct will have
    # two different definitions and Collector will crash when trying to connect to a grpc server.
    for file in $(grep -rl port_platform.h src/generated --include=*.h); do
        sed -i 's|#include <grpc/impl/codegen/port_platform.h>|#include <grpc/impl/codegen/port_platform.h>\n#ifdef GRPC_ASAN_ENABLED\n#  undef GRPC_ASAN_ENABLED\n#endif|' $file
    done
fi

echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig
mkdir -p cmake-collector
cd cmake-collector
cmake -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" -DADDRESS_SANITIZER="$ADDRESS_SANITIZER" ../src
make -j "${NPROCS:-2}" all

./runUnitTests

strip --strip-unneeded \
    ./collector \
    ./EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so
cd ..

### MODULE_VERSION

mkdir -p cmake-sysdig
cd cmake-sysdig
cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="-fno-pie" \
        -DPROBE_NAME=collector \
        -DBUILD_USERSPACE=OFF \
        -DBUILD_DRIVER=ON \
        -DENABLE_DKMS=OFF \
        -DBUILD_BPF=ON \
        ../sysdig/src
KERNELDIR=/dev/null make driver/fast 2> /dev/null || true
cd ../sysdig/src/driver
find . -type f \( -name 'Makefile' -o -name '*.c' -o -name '*.h' \) -print0 \
    | LC_ALL=C sort -z | xargs -0 sha256sum | awk '{print$1 " " $2}' | sha256sum | awk '{print$1}' \
    > /MODULE_VERSION.txt
cd ../../..
