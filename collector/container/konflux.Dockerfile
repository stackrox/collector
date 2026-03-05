ARG BUILD_DIR=/build
ARG CMAKE_BUILD_DIR=${BUILD_DIR}/cmake-build


FROM registry.access.redhat.com/ubi9/ubi:latest@sha256:6ed9f6f637fe731d93ec60c065dbced79273f1e0b5f512951f2c0b0baedb16ad AS builder

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


# Stage: ubi-micro base (needed for copying to /out to preserve rpmdb)
FROM registry.access.redhat.com/ubi9/ubi-micro:latest@sha256:093a704be0eaef9bb52d9bc0219c67ee9db13c2e797da400ddb5d5ae6849fa10 AS ubi-micro-base

# Stage: Package installer with runtime packages
FROM registry.access.redhat.com/ubi9/ubi:latest@sha256:6ed9f6f637fe731d93ec60c065dbced79273f1e0b5f512951f2c0b0baedb16ad AS package_installer

# Copy ubi-micro base to /out to preserve its rpmdb
COPY --from=ubi-micro-base / /out/

# Install packages directly to /out/ using --installroot
RUN dnf install -y \
    --installroot=/out/ \
    --releasever=9 \
    --setopt=install_weak_deps=False \
    --nodocs \
    --allowerasing \
    tbb c-ares crypto-policies-scripts elfutils-libelf ca-certificates openssl libuuid libstdc++ && \
    dnf clean all --installroot=/out/ && \
    rm -rf /out/var/cache/*

# Copy LICENSE into /out/ to consolidate layers
COPY LICENSE /out/licenses/LICENSE

# Copy builder artifacts into /out/
ARG CMAKE_BUILD_DIR
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/collector /out/usr/local/bin/collector
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/self-checks /out/usr/local/bin/self-checks


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

# Copy everything from package_installer in one layer (packages + LICENSE + binaries)
COPY --from=package_installer /out/ /

EXPOSE 8080 9090

ENTRYPOINT ["collector"]
