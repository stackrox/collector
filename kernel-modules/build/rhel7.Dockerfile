FROM quay.io/centos/centos:centos7

ENV DISTRO=rhel7

RUN yum makecache && \
    yum install -y centos-release-scl && \
    yum -y install \
        make \
        gcc-c++ \
        llvm-toolset-7.0 \
        elfutils-libelf \
        elfutils-libelf-devel \
        kmod

COPY /build-kos /scripts/
COPY /build-wrapper.sh /scripts/compile.sh

ENTRYPOINT ["scl", "enable", "llvm-toolset-7.0", "/scripts/compile.sh"]
