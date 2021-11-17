#!/usr/bin/env bash
set -e

set -ux
ldconfig -v
cd /tmp/cmake-build
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
ADDRESS_SANITIZER="${ADDRESS_SANITIZER:-false}"

if [ $ADDRESS_SANITIZER = "true" ]; then
  # Needed for address sanitizer to work. See https://github.com/grpc/grpc/issues/22238.
  # When Collector is built with address sanitizer it sets GRPC_ASAN_ENABLED, which changes a struct in the grpc library.
  # If grpc is compiled without that flag and is then linked with Collector the struct will have
  # two different definitions and Collector will crash when trying to connect to a grpc server.
  for file in `grep -rl port_platform.h /src/generated --include=*.h`
  do
  	sed -i 's|#include <grpc/impl/codegen/port_platform.h>|#include <grpc/impl/codegen/port_platform.h>\n#ifdef GRPC_ASAN_ENABLED\n#  undef GRPC_ASAN_ENABLED\n#endif|' $file
  done
fi

cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -DADDRESS_SANITIZER=$ADDRESS_SANITIZER /src
make -j "${NPROCS:-2}" all
if [ $CMAKE_BUILD_TYPE = "Release" ]; then
  echo "Strip unneeded"
  strip --strip-unneeded \
      ./collector \
      ./EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so
fi
cp -r /THIRD_PARTY_NOTICES .
