#!/usr/bin/env bash

# This builds the collector and its dependencies with the assumption that a Red
# Hat subscription provides access to required RHEL 8 RPMs and that dependencies
# are resolved with submodules.

set -eux

# shellcheck source=SCRIPTDIR/../install/versions.sh
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
COLLECTOR_APPEND_CID="${COLLECTOR_APPEND_CID:-false}"

cp -a collector/generated src/generated

if [ "$ADDRESS_SANITIZER" = "true" ]; then
    # Needed for address sanitizer to work. See https://github.com/grpc/grpc/issues/22238.
    # When Collector is built with address sanitizer it sets GRPC_ASAN_ENABLED, which changes a struct in the grpc library.
    # If grpc is compiled without that flag and is then linked with Collector the struct will have
    # two different definitions and Collector will crash when trying to connect to a grpc server.
    while read -r file; do
        sed -i 's|#include <grpc/impl/codegen/port_platform.h>|#include <grpc/impl/codegen/port_platform.h>\n#ifdef GRPC_ASAN_ENABLED\n#  undef GRPC_ASAN_ENABLED\n#endif|' "$file"
    done < <(grep -rl port_platform.h src/generated --include=*.h)
fi

echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig
mkdir -p cmake-collector
cd cmake-collector
cmake -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DADDRESS_SANITIZER="$ADDRESS_SANITIZER" \
    -DCOLLECTOR_APPEND_CID="$COLLECTOR_APPEND_CID" \
    ../src
make -j "${NPROCS:-2}" all

./runUnitTests

strip --strip-unneeded \
    ./collector \
    ./EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so
cd ..
