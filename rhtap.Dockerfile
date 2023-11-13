ARG BUILD_DIR=/build
ARG SRC_ROOT_DIR=${BUILD_DIR}
ARG CMAKE_BUILD_DIR=${SRC_ROOT_DIR}/cmake-build
ARG USE_VALGRIND=false
ARG ADDRESS_SANITIZER=false

# TODO(ROX-20312): we can't pin image tag or digest because currently there's no mechanism to auto-update that.
# TODO(ROX-20651): Use RHEL/ubi base image when entitlement is solved.
# RPMs requiring entitlement: bpftool, cmake-3.18.2-9.el8, elfutils-libelf-devel, tbb-devel, c-ares-devel, jq-devel
# Even when getting as many dependencies as possible through the WITH_RHEL_RPMS=false approve, with an unentitled ubi image,
# bpftool cmake-3.18.2-9.el8 elfutils-libelf-devel are still missing.
# FROM registry.access.redhat.com/ubi8/ubi:latest AS builder

# Can not use stream9, because cmake-3.18.2 is not available.
FROM quay.io/centos/centos:stream8 AS builder

# TODO: reduce COPY scope?
COPY . .

# TODO(ROX-20234): use hermetic builds when installing/updating RPMs becomes hermetic.
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
        # TODO(ROX-20651): if entitlement/RHEL base image is solved, this can be installed from default repository \
        # jq-devel
        c-ares-devel \
    && \
      # TODO(ROX-20651): if entitlement/RHEL base image is solved, this can be installed from default repository \
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

# Appends an argument to the driver download URL that is used for filtering alerts on missing kernels.
# TODO: This needs to be true on PRs only.
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

# WITH_RHEL_RPMS controls for dependency installation if they were already installed as RPMs.
# Setting the value to 'false' will cause dependencies to be downloaded (as archives or from repositories) and compiled.
# That is not possible with hermetic builds.
ENV WITH_RHEL_RPMS=true
RUN ./builder/install/install-dependencies.sh && \
    cmake -DDISABLE_PROFILING=ON -S "${SRC_ROOT_DIR}" -B "${CMAKE_BUILD_DIR}" && \
    cmake --build "${CMAKE_BUILD_DIR}" --target all -- -j "$(nproc)" && \
    (cd ${CMAKE_BUILD_DIR} && ctest -V) && \
    strip --strip-unneeded "${CMAKE_BUILD_DIR}/collector/collector" "${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so"

# TODO(ROX-20651): use entitled RHEL/ubi to access tbb and other devel tools
# FROM registry.access.redhat.com/ubi8/ubi-minimal:latest
FROM quay.io/centos/centos:stream8

WORKDIR /

LABEL \
    com.redhat.component="rhacs-collector-slim-container" \
    com.redhat.license_terms="https://www.redhat.com/agreements" \
    description="This image supports runtime data collection in the StackRox Kubernetes Security Platform" \
    io.k8s.description="This image supports runtime data collection in the StackRox Kubernetes Security Platform" \
    io.k8s.display-name="collector-slim" \
    io.openshift.tags="rhacs,collector,stackrox" \
    maintainer="Red Hat, Inc." \
    name="rhacs-collector-slim-rhel8" \
    source-location="https://github.com/stackrox/collector" \
    summary="Runtime data collection for the StackRox Kubernetes Security Platform" \
    url="https://catalog.redhat.com/software/container-stacks/detail/60eefc88ee05ae7c5b8f041c" \
    # We must set version label to prevent inheriting value set in the base stage.
    # TODO(ROX-20236): configure injection of dynamic version value when it becomes possible.
    version="0.0.1-todo" \
    collector_version="0.0.1-collector_version"

ARG BUILD_DIR
ARG CMAKE_BUILD_DIR

ENV COLLECTOR_HOST_ROOT=/host

COPY --from=builder ${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so /usr/local/lib/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/collector /usr/local/bin/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/self-checks /usr/local/bin/
COPY --from=builder ${BUILD_DIR}/collector/container/scripts /

# TODO(ROX-20651): use microdnf if we go back to ubi-minimal image after RHEL entitlement is solved
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
