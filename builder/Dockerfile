FROM debian:buster-slim

ARG NPROCS=6
ENV NPROCS=$NPROCS

# Install packaged dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        binutils \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        g++ \
        git \
        libcap-ng-dev \
        libcurl4-openssl-dev \
        libelf-dev \
        libssl-dev \
        libtool \
        libz-dev \
        pkg-config \
        python \
        unzip \
        uuid-dev \
        wget \
        ;

# We want to fail if the destination directory is there, hence mkdir (not -p).
RUN mkdir install-tmp

COPY install/*.sh install-tmp/

# Build dependencies from source
RUN ./install-tmp/install.sh

RUN ldconfig

# Copy files needed for generating proto code, building sysdig library and building collector
COPY build/protogen.mk /protogen.mk
COPY build/build-sysdig.sh /build-sysdig.sh
COPY build/build-collector.sh /build-collector.sh

RUN chmod 700 /build-collector.sh
RUN chmod 700 /build-sysdig.sh
