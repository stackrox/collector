#!/usr/bin/env sh
mkdir -p /tmp/src/build \
    && cd /tmp/src \
    && git clone https://github.com/curl/curl.git \
    && cd curl \
    && git checkout $CURL_REVISION \
    && cd .. \
    && cd build \
    && cmake ../curl \
    && make -j 6 \
    && make install \
    ;
