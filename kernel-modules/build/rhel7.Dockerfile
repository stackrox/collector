FROM quay.io/centos/centos:centos7

ENV DISTRO=rhel7

RUN yum makecache && \
    yum install -y centos-release-scl && \
    yum -y install epel-release && \
    yum -y install \
        make \
        cmake3 \
        gcc-c++ \
        llvm-toolset-7.0 \
        elfutils-libelf-devel \
        kmod && \
        ln -s /usr/bin/cmake3 /usr/bin/cmake

COPY /build-kos /scripts/
COPY /build-wrapper.sh /scripts/compile.sh

ENTRYPOINT ["scl", "enable", "llvm-toolset-7.0", "/scripts/compile.sh"]
