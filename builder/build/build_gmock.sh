#!/bin/sh

cd /usr/src/gmock \
    && cmake CMakeLists.txt \
    && make -j 6 \
    && cp libgmock*.a /usr/lib \
    && cp gtest/libgtest*.a /usr/lib