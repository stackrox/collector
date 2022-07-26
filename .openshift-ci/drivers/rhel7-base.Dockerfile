FROM quay.io/centos/centos:centos7

COPY --from=replaced-by-osci:scripts /scripts/ /scripts/

ENV DISTRO=rhel7

RUN yum makecache && \
    yum install -y centos-release-scl && \
    yum -y install \
        make \
        gcc-c++ \
        llvm-toolset-7.0 \
        elfutils-libelf \
        kmod && \
    /scripts/gcloud-sdk-install.sh
