FROM quay.io/centos/centos:stream8 as builder

ARG NPROCS=6
ENV NPROCS=$NPROCS

ARG BUILD_DIR=/install-tmp

USER root

RUN dnf -y update \
    && dnf -y install --nobest \
        autoconf \
        automake \
        binutils-devel \
        cmake-3.18.2-9.el8 \
        diffutils \
        elfutils-libelf-devel \
        file \
        gcc \
        gcc-c++ \
        git \
        glibc-devel \
        libasan \
        libcap-ng-devel \
        libcurl-devel \
        libtool \
        libuuid-devel \
        make \
        openssl-devel \
        pkgconfig \
        wget \
    && dnf clean all

# Build dependencies from source
WORKDIR ${BUILD_DIR}

COPY install builder/install
COPY build builder/build
COPY third_party third_party

RUN "builder/install/install-dependencies.sh" && \
    rm -rf ${BUILD_DIR} && \
    echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig

FROM builder as dev

RUN dnf -y update \
    && dnf -y install --nobest \
        bison \
        ca-certificates \
        clang \
        clang-tools-extra \
        flex \
        gdb \
        gettext \
        openssh-server \
        patchutils \
        passwd \
        rsync \
        tar \
        unzip \
        valgrind \
        which \
    && dnf clean all

# Create directory to copy collector source into builder container
RUN mkdir /src && chmod a+rwx /src
