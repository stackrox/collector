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
        elfutils-libelf-devel \
        kmod && \
    /scripts/gcloud-sdk-install.sh

ENV BASH_ENV="/scripts/llvm-toolset-7-enable" \
    ENV="/scripts/llvm-toolset-7-enable" \
    PROMPT_COMMAND=". /scripts/llvm-toolset-7-enable"
