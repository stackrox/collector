ARG BUILD_DIR=/build
ARG CMAKE_BUILD_DIR=${BUILD_DIR}/cmake-build


FROM registry.access.redhat.com/ubi9/ubi:latest@sha256:8805abe5b8a32c826d46926c069f20e6a7f854d59d5bd75c55e68278aea65ccc AS builder

RUN dnf -y install --nobest --allowerasing \
        make \
        wget \
        unzip \
        clang \
        llvm \
        cmake \
        gcc-c++ \
        openssl-devel \
        ncurses-devel \
        curl-devel \
        libuuid-devel \
        libcap-ng-devel \
        autoconf \
        libtool \
        git \
        elfutils-libelf-devel \
        tbb-devel \
        c-ares-devel \
        patch \
        # for USDT support
        systemtap-sdt-devel && \
    dnf -y clean all

ARG SOURCES_DIR=/staging

COPY . ${SOURCES_DIR}

ARG COLLECTOR_TAG
RUN if [[ "$COLLECTOR_TAG" == "" ]]; then >&2 echo "error: required COLLECTOR_TAG arg is unset"; exit 6; fi
ARG BUILD_DIR
ARG SRC_ROOT_DIR=${BUILD_DIR}
ARG CMAKE_BUILD_DIR
# TODO(ROX-20240): CMAKE_BUILD_TYPE should probably not be Release for PR, normal branch builds
ARG CMAKE_BUILD_TYPE=Release
# Appends an argument to the driver download URL that is used for filtering alerts on missing kernels.
# TODO(ROX-20240): This needs to be true on PRs only.
ARG USE_VALGRIND=false
ARG ADDRESS_SANITIZER=false
ARG TRACE_SINSP_EVENTS=false

WORKDIR ${BUILD_DIR}

RUN mkdir kernel-modules \
    && cp -a ${SOURCES_DIR}/builder builder \
    && ln -s builder/third_party third_party \
    && cp -a ${SOURCES_DIR}/collector collector \
    && cp -a ${SOURCES_DIR}/falcosecurity-libs falcosecurity-libs \
    && cp -a ${SOURCES_DIR}/CMakeLists.txt CMakeLists.txt

# WITH_RHEL_RPMS controls for dependency installation, ie if they were already installed as RPMs.
ENV WITH_RHEL_RPMS=true

# The following RUN commands are separated in order to make it easier
# to debug when a step fails.
RUN ./builder/install/install-dependencies.sh

# Build with gperftools (DISABLE_PROFILING=OFF) only for supported
# architectures, at the moment x86_64 only
RUN if [[ "$(uname -m)" == "x86_64" ]];   \
        then DISABLE_PROFILING="OFF";   \
        else DISABLE_PROFILING="ON";    \
    fi ; \
    cmake -S "${SRC_ROOT_DIR}" -B "${CMAKE_BUILD_DIR}" \
           -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
           -DDISABLE_PROFILING="${DISABLE_PROFILING}" \
           -DUSE_VALGRIND="${USE_VALGRIND}" \
           -DADDRESS_SANITIZER="${ADDRESS_SANITIZER}" \
           -DCOLLECTOR_VERSION="${COLLECTOR_TAG}" \
           -DTRACE_SINSP_EVENTS="${TRACE_SINSP_EVENTS}"
RUN cmake --build "${CMAKE_BUILD_DIR}" --target all -- -j "${NPROCS:-4}"
RUN ctest --no-tests=error -V --test-dir "${CMAKE_BUILD_DIR}"
RUN strip -v --strip-unneeded "${CMAKE_BUILD_DIR}/collector/collector"


FROM registry.access.redhat.com/ubi9/ubi-micro:latest@sha256:2173487b3b72b1a7b11edc908e9bbf1726f9df46a4f78fd6d19a2bab0a701f38 AS ubi-micro-base

FROM registry.access.redhat.com/ubi9/ubi:latest@sha256:8805abe5b8a32c826d46926c069f20e6a7f854d59d5bd75c55e68278aea65ccc AS package_installer

COPY --from=ubi-micro-base / /out/

# Install packages directly to /out/ using --installroot
# Note: --setopt=reposdir=/etc/yum.repos.d instructs dnf to use repo configurations pointing to RPMs
# prefetched by Hermeto/Cachi2, instead of installroot's default UBI repos.
RUN dnf install -y \
    --installroot=/out/ \
    --releasever=9 \
    --setopt=install_weak_deps=False \
    --setopt=reposdir=/etc/yum.repos.d \
    --nodocs \
    c-ares ca-certificates crypto-policies-scripts elfutils-libelf gzip less libcap-ng libcurl-minimal libstdc++ libuuid openssl tar tbb && \
    dnf clean all --installroot=/out/ && \
    rm -rf /out/var/cache/dnf /out/var/cache/yum


FROM ubi-micro-base

ARG COLLECTOR_TAG

WORKDIR /

LABEL \
    com.redhat.component="rhacs-collector-container" \
    com.redhat.license_terms="https://www.redhat.com/agreements" \
    description="This image supports runtime data collection for Red Hat Advanced Cluster Security for Kubernetes" \
    distribution-scope="public" \
    io.k8s.description="This image supports runtime data collection for Red Hat Advanced Cluster Security for Kubernetes" \
    io.k8s.display-name="collector" \
    io.openshift.tags="rhacs,collector,stackrox" \
    maintainer="Red Hat, Inc." \
    name="advanced-cluster-security/rhacs-collector-rhel9" \
    # Custom Snapshot creation in `operator-bundle-pipeline` depends on source-location label to be set correctly.
    source-location="https://github.com/stackrox/collector" \
    summary="Runtime data collection for Red Hat Advanced Cluster Security for Kubernetes" \
    url="https://catalog.redhat.com/software/container-stacks/detail/60eefc88ee05ae7c5b8f041c" \
    vendor="Red Hat, Inc." \
    # We must set version label for EC and to prevent inheriting value set in the base stage.
    version="${COLLECTOR_TAG}" \
    # Release label is required by EC although has no practical semantics.
    # We also set it to not inherit one from a base stage in case it's RHEL or UBI.
    release="1"

ARG BUILD_DIR
ARG CMAKE_BUILD_DIR

ENV COLLECTOR_HOST_ROOT=/host

COPY --from=package_installer /out/ /

COPY --from=builder ${CMAKE_BUILD_DIR}/collector/collector /usr/local/bin/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/self-checks /usr/local/bin/

COPY LICENSE /licenses/LICENSE

EXPOSE 8080 9090

ENTRYPOINT ["collector"]
