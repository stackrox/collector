FROM debian:stretch-slim

RUN printf "deb http://httpredir.debian.org/debian stretch-backports main non-free\ndeb-src http://httpredir.debian.org/debian stretch-backports main non-free" > /etc/apt/sources.list.d/backports.list

# Install our dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        binutils \
        build-essential \
        libssl-dev \
        libcurl4-openssl-dev \
        libluajit-5.1-dev \
        libncurses5-dev \
        libz-dev \
        libjsoncpp-dev \
        libb64-dev \
        cmake \
        `# For proper HTTPS validation in git clone:` \
        ca-certificates \
        `# For sysdig to pull a `jq` patch:` \
        wget \
        curl \
        libtbb-dev \
        libelf-dev \
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

COPY build.sh /build.sh

RUN chmod 700 /build.sh

# Build
CMD /build.sh
