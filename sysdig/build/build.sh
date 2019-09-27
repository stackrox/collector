#! /bin/sh
set -eux

mkdir -p /sysdig-build
cd /sysdig-build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DRIVER=OFF \
    -DUSE_BUNDLED_DEPS=ON \
    -DUSE_BUNDLED_ZLIB=OFF \
    -DUSE_BUNDLED_NCURSES=OFF \
    -DUSE_BUNDLED_OPENSSL=OFF \
    -DUSE_BUNDLED_B64=OFF \
    -DUSE_BUNDLED_TBB=OFF \
    -DUSE_BUNDLED_CURL=OFF \
    -DUSE_BUNDLED_JSONCPP=OFF \
    -DUSE_BUNDLED_LUAJIT=OFF \
    -DUSE_BUNDLED_CARES=OFF \
    -DUSE_BUNDLED_PROTOBUF=OFF \
    -DUSE_BUNDLED_GRPC=OFF \
    /sysdig-src
make -j6
strip --strip-unneeded ./userspace/libsinsp/libsinsp-wrapper.so
