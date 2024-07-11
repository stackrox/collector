FROM oraclelinux:7

RUN yum -y update && yum -y install yum-utils && \
    yum-config-manager --enable ol7_UEKR6 && \
    yum -y install \
    gcc \
    gcc-c++ \
    autoconf \
    file \
    make \
    cmake \
    libdtrace-ctf \
    elfutils-libelf-devel && \
    yum-config-manager --add-repo=http://yum.oracle.com/public-yum-ol7.repo && \
    yum-config-manager --enable ol7_developer --enable ol7_developer_EPEL && \
    yum install -y rh-dotnet20-clang rh-dotnet20-llvm

COPY build-kos /scripts/
COPY build-wrapper.sh /scripts/compile.sh

ENTRYPOINT ["scl", "enable", "rh-dotnet20", "/scripts/compile.sh"]
