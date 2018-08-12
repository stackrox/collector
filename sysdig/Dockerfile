FROM debian:jessie-slim

# Install our dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        gcc-4.8 \
        g++-4.8 \
        # For checking out librdkafka:
        git \
        # For proper HTTPS validation in git clone:
        ca-certificates \
        # For the librdkafka build:
        python \
        # For sysdig to pull a `jq` patch:
        wget \
        ;

# We want to fail if the destination directory is there, hence mkdir (not -p).
RUN mkdir build-tmp

ARG RDKAFKA_REVISION=v0.11.3
COPY build/install_rdkafka.sh build-tmp/install_rdkafka.sh
RUN build-tmp/install_rdkafka.sh

# Clean up the build directory we created.
RUN rm -rf build-tmp

COPY build/build.sh /build.sh

RUN chmod 700 /build.sh

# Build
CMD /build.sh
