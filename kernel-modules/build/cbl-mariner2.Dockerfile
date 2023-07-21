FROM --platform=linux/amd64 mcr.microsoft.com/cbl-mariner/base/core:2.0 AS cbl-mariner-2.0-base

RUN tdnf -y update && \
    tdnf -y install \
        binutils \
        clang \
        cmake \
        elfutils-libelf-devel \
        gcc-c++ \
        glibc-devel \
        kernel-headers \
        kernel-devel \
        kmod \
        llvm \
        make && \
    ln -s /usr/bin/gcc /usr/bin/gcc-4.9 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-6 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-8 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-9 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-10

COPY /build-kos /scripts/
COPY /build-wrapper.sh /scripts/compile.sh

ENTRYPOINT /scripts/compile.sh