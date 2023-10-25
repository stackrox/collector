ARG BUILD_DIR=/build
ARG SRC_ROOT_DIR=${BUILD_DIR}
ARG CMAKE_BUILD_DIR=${SRC_ROOT_DIR}/cmake-build
ARG USE_VALGRIND=false
ARG ADDRESS_SANITIZER=false

# TODO: follow tags
# TODO: use entitled RHEL/ubi to access cmake and other devel tools
# FROM registry.access.redhat.com/ubi8/ubi:latest AS builder
FROM quay.io/centos/centos:stream8 AS builder

# TODO: reduce COPY scope
COPY . .

# TODO: prefetch dependencies for hermetic builds
RUN dnf -y update && \
    dnf -y install \
        make \
        wget \
        unzip \
        clang \
        bpftool \
        cmake-3.18.2-9.el8 \
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
        # jq-devel # TODO: if entitlement/RHEL base image is solved, this can be installed from default repository \
        c-ares-devel \
    && \
      # TODO: if entitlement/RHEL base image is solved, this can be installed from default repository \
      dnf --enablerepo=powertools -y install jq-devel \
    && \
    dnf clean all

ARG BUILD_DIR
ENV BUILD_DIR=${BUILD_DIR}

ARG SRC_ROOT_DIR
ENV SRC_ROOT_DIR=${SRC_ROOT_DIR}

ARG CMAKE_BUILD_DIR
ENV CMAKE_BUILD_DIR=${CMAKE_BUILD_DIR}

# TODO: CMAKE_BUILD_TYPE should probably not be Release for PR, normal branch builds
ARG CMAKE_BUILD_TYPE=Release
ENV CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}

# TODO: what does this do?
ARG COLLECTOR_APPEND_CID=false
ENV COLLECTOR_APPEND_CID=${COLLECTOR_APPEND_CID}

RUN mkdir -p ${BUILD_DIR}
WORKDIR ${BUILD_DIR}
RUN cp -a /builder builder
RUN cp -a /collector collector
RUN cp -a /falcosecurity-libs falcosecurity-libs
RUN cp -a /builder/third_party third_party
RUN mkdir kernel-modules
RUN cp -a /kernel-modules/MODULE_VERSION kernel-modules/MODULE_VERSION
RUN cp -a /CMakeLists.txt CMakeLists.txt

ENV DISABLE_PROFILING=true
# TODO: this is an old comment, understand it...
# # TODO: Remove this variable once upstream is fully merged
ENV WITH_RHEL8_RPMS=true
ENV WITH_RHEL_RPMS=true
RUN ./builder/install/install-dependencies.sh && \
    ./builder/build/build-collector.sh && \
    "${CMAKE_BUILD_DIR}/collector/runUnitTests"

# TODO: use entitled RHEL/ubi to access tbb and other devel tools
# FROM registry.access.redhat.com/ubi8/ubi-minimal:latest
FROM quay.io/centos/centos:stream8

WORKDIR /

# TODO: set labels
LABEL \
    com.redhat.component="rhacs-collector-slim-container" \
    name="rhacs-collector-slim-rhel8" \
    maintainer="Red Hat, Inc." \
    version="${CI_VERSION}" \
    "git-commit:stackrox/collector"="${CI_COLLECTOR_UPSTREAM_COMMIT}" \
    "git-branch:stackrox/collector"="${CI_COLLECTOR_UPSTREAM_BRANCH}" \
    "git-tag:stackrox/collector"="${CI_COLLECTOR_UPSTREAM_TAG}"

# TODO: set label
ARG COLLECTOR_VERSION=0.0.1-todo
LABEL collector_version="${COLLECTOR_VERSION}"

ARG BUILD_DIR
ARG CMAKE_BUILD_DIR

ENV COLLECTOR_HOST_ROOT=/host

COPY --from=builder ${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so /usr/local/lib/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/collector /usr/local/bin/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/self-checks /usr/local/bin/
COPY --from=builder ${BUILD_DIR}/collector/container/scripts /

# TODO: use microdnf if we go back to ubi-minimal image
RUN mv /collector-wrapper.sh /usr/local/bin/ && \
    chmod 700 bootstrap.sh && \
    echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && \
    ldconfig && \
    mkdir /kernel-modules && \
    dnf upgrade -y --nobest && \
    dnf install -y \
      kmod \
      tbb \
      jq \
      c-ares && \
    dnf clean all && \
    rpm --verbose -e --nodeps $(rpm -qa 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*') && \
    rm -rf /var/cache/dnf /var/cache/yum

COPY --from=builder ${BUILD_DIR}/kernel-modules/MODULE_VERSION /kernel-modules/MODULE_VERSION.txt

EXPOSE 8080 9090

ENTRYPOINT ["/bootstrap.sh"]

# hadolint ignore=DL3025
CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER
