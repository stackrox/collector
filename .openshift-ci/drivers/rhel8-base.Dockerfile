FROM quay.io/centos/centos:stream8 AS rhel-8-base

COPY --from=replaced-by-osci:scripts /scripts/ /scripts/

ENV DISTRO=rhel8

RUN dnf -y update && \
    dnf -y install \
        make \
        cmake \
        gcc-c++ \
        llvm \
        clang \
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
