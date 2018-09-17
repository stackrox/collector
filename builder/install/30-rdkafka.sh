#!/usr/bin/env bash

set -e

git clone https://github.com/edenhill/librdkafka.git -b $RDKAFKA_REVISION --depth 1
cd librdkafka
./configure --disable-sasl
make -j 6
make install
