FROM stackrox/kernel-modules:latest
# This is the same base image as the collector image itself.
# Inheriting from it ensures that we will have the same version of Debian,
# including packages, which is especially important for dynamically-linked
# libraries.

# Install our dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        apt-transport-https \
        autoconf \
        automake \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        g++-4.8 \
        google-mock \
        libjsoncpp-dev \
        libssl-dev \
        libtool \
        unzip \
        uuid-dev \
        libcap-ng-dev \
        ;

# We want to fail if the destination directory is there, hence mkdir (not -p).
RUN mkdir build-tmp

# prometheus-cpp
COPY build/install_prometheus.sh build-tmp/install_prometheus.sh
RUN build-tmp/install_prometheus.sh

# GoogleMock (and GoogleTest) are opinionated about how to install the library
RUN cd /usr/src/gmock \
        && cmake CMakeLists.txt \
        && make -j 6 \
        && cp libgmock*.a /usr/lib \
        && cp gtest/libgtest*.a /usr/lib \
        ;

# cURL 7.42 or higher is required for UNIX socket access
ARG CURL_REVISION=curl-7_57_0
COPY build/install_libcurl.sh build-tmp/install_libcurl.sh
RUN build-tmp/install_libcurl.sh

ARG RDKAFKA_REVISION=v0.11.3
COPY build/install_rdkafka.sh build-tmp/install_rdkafka.sh
RUN build-tmp/install_rdkafka.sh

# Clean up the build directory we created.
RUN rm -rf build-tmp

# Prepare everything, build, and run tests
COPY build/build.sh /build.sh

RUN chmod 700 /build.sh
CMD /build.sh
