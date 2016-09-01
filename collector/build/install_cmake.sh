#!/usr/bin/env sh
wget -q https://cmake.org/files/v3.5/cmake-3.5.2-Linux-x86_64.sh \
    && chmod +x cmake-3.5.2-Linux-x86_64.sh \
    && ./cmake-3.5.2-Linux-x86_64.sh --skip-license