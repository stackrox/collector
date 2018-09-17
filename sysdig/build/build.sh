#! /bin/sh
set -eux

mkdir -p /sysdig-src/build
cd /sysdig-src/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DRIVER=OFF \
    -DUSE_BUNDLED_DEPS=ON \
    -DUSE_BUNDLED_ZLIB=OFF \
    -DUSE_BUNDLED_NCURSES=OFF \
    -DUSE_BUNDLED_OPENSSL=OFF \
    -DUSE_BUNDLED_CURL=OFF \
    -DUSE_BUNDLED_LUAJIT=OFF \
    ..
make
strip --strip-unneeded ./userspace/libsinsp/libsinsp-wrapper.so
