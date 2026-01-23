FROM quay.io/centos/centos:stream10

ARG BUILD_DIR=/install-tmp
ARG COLLECTOR_BUILDER_DEBUG

USER root

ENV OUTPUT_DIR=/tmp/output

RUN dnf -y update \
    && dnf -y install --nobest \
        autoconf \
        automake \
        binutils-devel \
        bison \
        ca-certificates \
        clang \
        llvm \
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
        libubsan \
        libcap-ng-devel \
        libcurl-devel \
        libtool \
        libuuid-devel \
        make \
        openssl-devel \
        patchutils \
        passwd \
        pkgconfig \
        rsync \
        tar \
        unzip \
        valgrind \
        wget \
        which \
        # for USDT support
        systemtap-sdt-devel \
    && dnf clean all

# Build dependencies from source
WORKDIR ${BUILD_DIR}

COPY install builder/install
COPY third_party third_party

RUN --mount=type=cache,target=${OUTPUT_DIR} \
    "builder/install/install-dependencies.sh" \
    && if [ -z "${COLLECTOR_BUILDER_DEBUG}" ]; then rm -rf ${BUILD_DIR}; fi

# Create directory to copy collector source into builder container
RUN mkdir /src && chmod a+rwx /src
