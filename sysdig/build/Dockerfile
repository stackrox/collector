FROM debian:stretch-slim

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
        protobuf-compiler \
        libtbb-dev \
        libelf-dev \
        ;

COPY build.sh /build.sh

RUN chmod 700 /build.sh

# Build
CMD /build.sh
