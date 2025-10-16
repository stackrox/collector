FROM quay.io/centos/centos:stream9

ARG BUILD_DIR=/install-tmp

USER root

RUN dnf -y update \
    && dnf -y install --nobest \
        autoconf \
        automake \
        binutils-devel \
        bison \
        ca-certificates \
        'clang-19.1.*' \
        'llvm-19.1.*' \
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

RUN "builder/install/install-dependencies.sh" && \
    rm -rf ${BUILD_DIR} && \
    echo -e '/usr/local/lib\n/usr/local/lib64' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig

# Create directory to copy collector source into builder container
RUN mkdir /src && chmod a+rwx /src
