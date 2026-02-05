ARG BUILD_DIR=/build
ARG CMAKE_BUILD_DIR=${BUILD_DIR}/cmake-build


FROM registry.access.redhat.com/ubi8/ubi:latest@sha256:bf6868a6f7ca34ea53d8b0ba01cbcee5af44d359732be84e3d1185d4aecb2a8e AS builder

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


FROM registry.access.redhat.com/ubi8/ubi-minimal:latest@sha256:000dd8e4a3046ac7c47e65bbe01efc48d7a568e5ee9946cca1d74a7abf042d36

RUN microdnf -y install --nobest \
      tbb \
      c-ares && \
    microdnf -y clean all && \
    rpm --verbose -e --nodeps $(rpm -qa 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*' 'libyaml*' 'libarchive*') && \
    rm -rf /var/cache/dnf /var/cache/yum

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
    name="advanced-cluster-security/rhacs-collector-rhel8" \
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

COPY --from=builder ${CMAKE_BUILD_DIR}/collector/collector /usr/local/bin/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/self-checks /usr/local/bin/

COPY LICENSE /licenses/LICENSE

EXPOSE 8080 9090

ENTRYPOINT ["collector"]
