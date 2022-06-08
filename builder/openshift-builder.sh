#!/usr/bin/env bash
set -eo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

dnf -y update \
&& dnf -y install \
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

# We want to fail if the destination directory is there, hence mkdir (not -p).
mkdir "${ROOT}/builder/install-tmp"

echo "root= $ROOT"

echo; echo;
ls "$ROOT"
echo; echo;
ls "$ROOT/builder"
echo; echo;
ls "$ROOT/builder/install"
echo; echo;
ls "$ROOT/builder/install/*.sh"

cp "${ROOT}/builder/install/*.sh" "${ROOT}/builder/install-tmp/"

# Build dependencies from source
"${ROOT}/builder/install-tmp/install.sh"

echo '/usr/local/lib' >/etc/ld.so.conf.d/usrlocallib.conf && ldconfig

# Copy script for building collector
cp "${ROOT}/builder/build/build-collector.sh" /
chmod 700 /build-collector.sh

# Create directory to copy collector source into builder container
mkdir /src && chmod a+rwx /src
