FROM registry.fedoraproject.org/fedora:38 AS builder

ENV DISTRO=fc38

RUN dnf -y install \
        make \
        cmake \
        gcc-c++ \
        llvm \
        clang \
        elfutils-libelf \
        elfutils-libelf-devel \
        kmod

COPY /build-kos /scripts/
COPY /build-wrapper.sh /scripts/compile.sh

ENTRYPOINT /scripts/compile.sh
