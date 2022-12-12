FROM registry.fedoraproject.org/fedora:36 AS builder

ENV DISTRO=fc36

RUN dnf -y install \
        make \
        gcc-c++ \
        llvm \
        clang \
        elfutils-libelf \
        elfutils-libelf-devel \
        kmod  && \
    # We trick Debian builds into thinking they have the required GCC binary
    ln -s /usr/bin/gcc /usr/bin/gcc-10 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-11 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-12

COPY /build-kos /scripts/
COPY /build-wrapper.sh /scripts/compile.sh

ENTRYPOINT /scripts/compile.sh
