FROM registry.fedoraproject.org/fedora:36 AS builder

ENV DISTRO=fc36

RUN dnf -y install \
        make \
        cmake \
        gcc-c++ \
        llvm \
        clang \
        patch \
        elfutils-libelf \
        elfutils-libelf-devel \
        git \
        python3 \
        kmod \
        which \
        libxcrypt-compat.x86_64 && \
    # We trick Debian builds into thinking they have the required GCC binary
    ln -s /usr/bin/gcc /usr/bin/gcc-10 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-11 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-12
