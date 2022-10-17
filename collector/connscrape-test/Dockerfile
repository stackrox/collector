FROM quay.io/centos/centos:stream8

USER root

RUN dnf -y update \
    && dnf -y install --nobest \
        autoconf \
        automake \
        binutils-devel \
        bison \
        ca-certificates \
        clang \
        cmake \
        cracklib-dicts \
        diffutils \
        elfutils-libelf-devel \
        file \
        flex \
        gcc \
        gcc-c++ \
        gdb \
        gettext \
        git \
        glibc-devel \
        libasan \
        libcap-ng-devel \
        libcurl-devel \
        libtool \
        libuuid-devel \
        make \
        nc \
        openssh-server \
        openssl-devel \
        patchutils \
        passwd \
        pkgconfig \
        python2 \
        rsync \
        tar \
        unzip \
        valgrind \
        wget \
        which \
    && dnf clean all

