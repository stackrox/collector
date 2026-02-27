FROM quay.io/centos/centos:stream10

ARG BUILD_DIR=/install-tmp
ARG COLLECTOR_BUILDER_DEBUG

USER root

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
        rpm-devel \
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
COPY orphaner orphaner

RUN "builder/install/install-dependencies.sh" && if [ -z "${COLLECTOR_BUILDER_DEBUG}" ]; then rm -rf ${BUILD_DIR}; fi
RUN echo -e '/usr/local/lib\n/usr/local/lib64' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig

# Create directory to copy collector source into builder container
RUN mkdir /src && chmod a+rwx /src
