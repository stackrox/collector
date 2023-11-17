ARG BUILD_DIR=/build
ARG SRC_ROOT_DIR=${BUILD_DIR}
ARG CMAKE_BUILD_DIR=${SRC_ROOT_DIR}/cmake-build
ARG USE_VALGRIND=false
ARG ADDRESS_SANITIZER=false

# TODO(ROX-20312): we can't pin image tag or digest because currently there's no mechanism to auto-update that.
# TODO(ROX-20651): Use RHEL/ubi base image when entitlement is solved.
# RPMs requiring entitlement: bpftool, cmake-3.18.2-9.el8, elfutils-libelf-devel, tbb-devel, c-ares-devel, jq-devel
# FROM registry.access.redhat.com/ubi8/ubi:latest AS builder
FROM quay.io/centos/centos:stream9 AS builder

# TODO: reduce COPY scope?
COPY . .

# TODO(ROX-20234): use hermetic builds when installing/updating RPMs becomes hermetic.
RUN dnf -y update \
    && dnf -y install --nobest \
        autoconf \
        automake \
        binutils-devel \
        bison \
        ca-certificates \
        clang-15.0.7 \
        cmake \
        cracklib-dicts \
        diffutils \
        elfutils-libelf-devel \
        file \
        flex \
        gcc \
        gcc-c++ \
        gdb \
        gettext \
        git \
        glibc-devel \
        libasan \
        libubsan \
        libcap-ng-devel \
        libcurl-devel \
        libtool \
        libuuid-devel \
        make \
        openssh-server \
        openssl-devel \
        patchutils \
        passwd \
        pkgconfig \
        rsync \
        tar \
        unzip \
        valgrind \
        wget \
        which \
        bpftool \
    && dnf clean all

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

WORKDIR ${BUILD_DIR}
RUN cp -a /builder builder \
    && cp -a /collector collector \
    && cp -a /falcosecurity-libs falcosecurity-libs \
    && cp -a /builder/third_party third_party \
    && mkdir kernel-modules \
    && cp -a /kernel-modules/MODULE_VERSION kernel-modules/MODULE_VERSION \
    && cp -a /CMakeLists.txt CMakeLists.txt

# WITH_RHEL_RPMS controls for dependency installation if they were already installed as RPMs.
# Setting the value to empty will cause dependencies to be downloaded (as archives or from repositories) and compiled.
# That is not possible with hermetic builds.
# ENV WITH_RHEL_RPMS=true
RUN ./builder/install/install-dependencies.sh \
    && cmake -DDISABLE_PROFILING=ON -S "${SRC_ROOT_DIR}" -B "${CMAKE_BUILD_DIR}" \
    && cmake --build "${CMAKE_BUILD_DIR}" --target all -- -j "$(nproc)" \
    && (cd ${CMAKE_BUILD_DIR} && ctest -V) \
    && strip --strip-unneeded "${CMAKE_BUILD_DIR}/collector/collector" "${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so"

FROM registry.access.redhat.com/ubi9/ubi-minimal:9.2

ARG BUILD_TYPE=rhel
ARG ROOT_DIR=.
ARG COLLECTOR_VERSION
ARG MODULE_VERSION

ENV ROOT_DIR=$ROOT_DIR
ENV COLLECTOR_VERSION="${COLLECTOR_VERSION}"

# TODO: must be set, propagated to /kernel-modules/MODULE_VERSION.txt, also coming from kernel-modules/MODULE_VERSION.txt, so what's the point?
ENV MODULE_VERSION="${MODULE_VERSION}"
ENV COLLECTOR_HOST_ROOT=/host

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
    version=${COLLECTOR_VERSION} \
    collector_version=${COLLECTOR_VERSION}
    # These two labels only exist on upstream
    # io.stackrox.collector.module-version="${MODULE_VERSION}" \
    # io.stackrox.collector.version="${COLLECTOR_VERSION}"

WORKDIR /

COPY collector/container/${BUILD_TYPE}/install.sh /
RUN ./install.sh && rm -f install.sh

COPY collector/container/scripts/collector-wrapper.sh /usr/local/bin
COPY collector/container/scripts/bootstrap.sh /
COPY collector/LICENSE-kernel-modules.txt /kernel-modules/LICENSE
COPY kernel-modules/MODULE_VERSION /kernel-modules/MODULE_VERSION.txt
COPY --from=builder /build/cmake-build/collector/collector /usr/local/bin/collector
COPY --from=builder /build/cmake-build/collector/self-checks /usr/local/bin/self-checks
COPY --from=builder /THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY --from=builder /collector/NOTICE-sysdig.txt /THIRD_PARTY_NOTICES/sysdig
COPY --from=builder /build/cmake-build/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so /usr/local/lib/

RUN echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && \
    ldconfig && \
    chmod 700 bootstrap.sh

EXPOSE 8080 9090

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER
