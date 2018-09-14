#!/usr/bin/env sh
git clone https://github.com/edenhill/librdkafka.git \
    && cd librdkafka \
    && git checkout $RDKAFKA_REVISION \
    && ./configure --disable-sasl \
    && make -j 6 \
    && make install \
    ;
