#!/usr/bin/env bash
set -eux

export CPAAS_BUILD="true"
export LICENSE_DIR="/THIRD_PARTY_NOTICES"
mkdir -p "${LICENSE_DIR}"

### Generate source from protobufs

cd third_party
GRPC_VERSION=v1.28.1 ../builder/install/40-grpc.sh
cd ..

ln -s third_party/googleapis googleapis

BASE_PATH=$(pwd) make -f builder/build/protogen.mk SKIP_GOOGLEAPI_FETCH=1 generated-proto-srcs

### Dependencies

export B64_VERSION=1.2.1
export CARES_VERSION=1.16.0
export CMAKE_VERSION=3.15.2
export ELFUTILS_VERSION=0.182
export GOOGLETEST_REVISION=release-1.10.0
export GRPC_REVISION=v1.28.1
export JQ_VERSION=1.6
export JSONCPP_REVISION=0.10.7
export LUAJIT_VERSION=2.0.3
export NCURSES_VERSION=6_0_20150725
export PROMETHEUS_CPP_REVISION=v0.9.0
export PROTOBUF_VERSION=3.11.4
export TBB_VERSION=2018_U5

cd third_party
../builder/install/50-libb64.sh
../builder/install/50-luajit.sh
../builder/install/50-jq.sh .
../builder/install/50-prometheus.sh .
cd ..

### Build

cp -a collector/generated src/generated
echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig
mkdir -p cmake-collector
cd cmake-collector
cmake -DCMAKE_BUILD_TYPE=Release ../src
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
find . -type f \( -name 'Makefile' -o -name '*.c' -o -name '*.h' \) -print0 | \
    LC_ALL=C sort -z | xargs -0 sha256sum | awk '{print$1 " " $2}' | sha256sum | awk '{print$1}' \
    > /MODULE_VERSION.txt
cd ../../..
