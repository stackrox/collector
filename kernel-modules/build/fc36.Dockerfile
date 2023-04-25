FROM registry.fedoraproject.org/fedora:36 AS builder

ENV DISTRO=fc36

RUN dnf -y install --nobest \
        make \
        cmake \
        gcc-c++ \
        llvm \
        clang \
        elfutils-libelf \
        elfutils-libelf-devel \
        kmod  && \
    # We trick Debian builds into thinking they have the required GCC binary
    ln -s /usr/bin/gcc /usr/bin/gcc-10 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-11 && \
    ln -s /usr/bin/gcc /usr/bin/gcc-12 && \
    # We also trick Amazon Linux
    ln -s /usr/bin/gcc /usr/bin/gcc10-gcc && \
    ln -s /usr/bin/ld.bfd /usr/bin/gcc10-ld.bfd && \
    ln -s /usr/bin/objdump /usr/bin/gcc10-objdump

COPY /build-kos /scripts/
COPY /build-wrapper.sh /scripts/compile.sh

ENTRYPOINT /scripts/compile.sh
