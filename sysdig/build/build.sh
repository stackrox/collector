#! /bin/sh
set -eux

mkdir -p /sysdig-build
cd /sysdig-build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DRIVER=OFF \
    -DUSE_BUNDLED_DEPS=ON \
    -DUSE_BUNDLED_ZLIB=OFF \
    -DUSE_BUNDLED_NCURSES=OFF \
    -DUSE_BUNDLED_OPENSSL=OFF \
    -DUSE_BUNDLED_CURL=OFF \
    -DUSE_BUNDLED_LUAJIT=OFF \
    /sysdig-src
make --debug=bm
strip --strip-unneeded ./userspace/libsinsp/libsinsp-wrapper.so
