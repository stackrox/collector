#!/usr/bin/env sh
set -eux

git clone https://github.com/google/protobuf.git
cd protobuf
git checkout v3.2.0
./autogen.sh
sh ./configure
make -j 6
make install
ldconfig

git clone https://github.com/jupp0r/prometheus-cpp.git
cd prometheus-cpp
git checkout 871a7673772b266135cc8422490578da1cf63004

cd 3rdparty

git clone https://github.com/civetweb/civetweb.git
cd civetweb
git checkout fbdee74
cp -R include /usr/local/include/civetweb
cd ../

git clone https://github.com/google/googletest.git
cd googletest
git checkout release-1.8.0
cd ../

git clone https://github.com/prometheus/client_model.git prometheus_client_model
cd prometheus_client_model
git checkout e2da43a
cd ../

cd ../

mkdir build
cd build
cmake ../
make -j 6
make install
ldconfig

