#! /bin/sh
set -eux

PATH=/usr/lib/ccache:$PATH
sudo mkdir -p /sysdig-build && sudo chmod 777 /sysdig-build
cd /sysdig-build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DRIVER=OFF \
    -DUSE_BUNDLED_DEPS=ON \
    -DUSE_BUNDLED_ZLIB=OFF \
    -DUSE_BUNDLED_NCURSES=OFF \
    -DUSE_BUNDLED_OPENSSL=OFF \
    -DUSE_BUNDLED_CURL=OFF \
    -DUSE_BUNDLED_LUAJIT=OFF \
    /builder/sysdig/src
make -j6
strip --strip-unneeded ./userspace/libsinsp/libsinsp-wrapper.so
