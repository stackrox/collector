FROM quay.io/centos/centos:stream8 AS rhel-8-base

COPY --from=replaced-by-osci:scripts /scripts/ /scripts/

ENV DISTRO=rhel8

ARG CLANG_VERSION=11.0.0-1.module_el8.4.0+587+5187cac0
ARG LLVM_VERSION=11.0.0-2.module_el8.4.0+587+5187cac0
#ARG CLANG_VERSION=12.0.0-1.module_el8.5.0+840+21214faf
#ARG LLVM_VERSION=12.0.0-1.module_el8.5.0+840+21214faf

ENV CLANG_VERSION="${CLANG_VERSION}"
ENV LLVM_VERSION="${LLVM_VERSION}"

RUN dnf -y update && \
    dnf -y install \
        make \
        cmake \
        gcc-c++ \
        llvm-${LLVM_VERSION} \
        clang-${CLANG_VERSION} \
        elfutils-libelf \
        elfutils-libelf-devel \
        kmod && \
    # We trick Debian builds into thinking they have the required GCC binaries
    ln -s /usr/bin/gcc /usr/bin/gcc-4.9 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-6 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-8 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-9 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-10 && \
    /scripts/gcloud-sdk-install.sh
