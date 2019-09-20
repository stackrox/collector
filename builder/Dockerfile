FROM debian:stretch-slim

RUN printf "deb http://httpredir.debian.org/debian stretch-backports main non-free\ndeb-src http://httpredir.debian.org/debian stretch-backports main non-free" > /etc/apt/sources.list.d/backports.list

# Install our dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        binutils \
        build-essential \
        g++ \
        ca-certificates \
        cmake \
        curl \
        python \
        libcurl4-openssl-dev \
        git \
        libb64-dev \
        libz-dev \
        libssl-dev \
        libtool \
        unzip \
        uuid-dev \
        libcap-ng-dev \
        libluajit-5.1 \
        libtbb-dev \
        libelf-dev \
        libjsoncpp-dev \
        pkg-config \
        ;
RUN apt-get update \
    && apt-get install -y --no-install-recommends -t stretch-backports \
        libc-ares-dev \
        libprotobuf-dev \
        protobuf-compiler \
        libgrpc++-dev \
        protobuf-compiler-grpc \
        ;

# We want to fail if the destination directory is there, hence mkdir (not -p).
RUN mkdir install-tmp

ARG GOOGLETEST_REVISION=release-1.8.1
ARG PROTOBUF_VERSION=3.6.1
ARG GRPC_REVISION=v1.15.0
ARG PROMETHEUS_CPP_REVISION=v0.4.2
ARG JSONCPP_REVISION=1.8.4

COPY install/*.sh install-tmp/
RUN ( cd install-tmp/ ; for f in *.sh; do ./"$f" ; done; )

# Clean up the build directory we created.
RUN rm -rf install-tmp

RUN ldconfig

# Prepare everything, build, and run tests
COPY build/build.sh /build.sh

RUN chmod 700 /build.sh
CMD /build.sh
