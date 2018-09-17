FROM debian:stretch-slim

# Install our dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        libssl-dev \
        libcurl4-openssl-dev \
        libluajit-5.1-dev \
        libncurses5-dev \
        libz-dev \
        libjsoncpp-dev \
        cmake \
        # For proper HTTPS validation in git clone:
        ca-certificates \
        # For sysdig to pull a `jq` patch:
        wget \
        ;

# We want to fail if the destination directory is there, hence mkdir (not -p).
RUN mkdir build-tmp

# Clean up the build directory we created.
RUN rm -rf build-tmp

COPY build/build.sh /build.sh

RUN chmod 700 /build.sh

# Build
CMD /build.sh
