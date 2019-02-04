#!/usr/bin/env bash
PATH=/usr/lib/ccache:$PATH
set -e

cd
PROMETHEUS_CPP_REVISION=v0.4.2
git clone https://github.com/jupp0r/prometheus-cpp.git -b "$PROMETHEUS_CPP_REVISION" --depth 1
cd prometheus-cpp
git submodule update --init
sudo mkdir -p /usr/local/include
sudo cp -R 3rdparty/civetweb/include /usr/local/include/civetweb

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make -j 6
sudo make install
sudo ldconfig

