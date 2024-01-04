ARG BUILD_DIR=/build
ARG CMAKE_BUILD_DIR=${BUILD_DIR}/cmake-build

# Builder
# TODO(ROX-20312): we can't pin image tag or digest because currently there's no mechanism to auto-update that.
# TODO(ROX-20651): Use RHEL/ubi base image when entitlement is solved.
# RPMs requiring entitlement: bpftool, cmake-3.18.2-9.el8, elfutils-libelf-devel, tbb-devel, c-ares-devel, jq-devel
# FROM registry.access.redhat.com/ubi8/ubi:latest AS builder
FROM registry.access.redhat.com/ubi8/ubi:latest AS ubi-normal
FROM registry.access.redhat.com/ubi8/ubi:latest AS rpm-implanter-builder

COPY --from=ubi-normal / /mnt
COPY ./.rhtap /tmp/.rhtap

# TODO(ROX-20234): use hermetic builds when installing/updating RPMs becomes hermetic.
RUN /tmp/.rhtap/scripts/subscription-manager-bro.sh register && \
    dnf -y --installroot=/mnt upgrade --nobest && \
    dnf -y --installroot=/mnt install --nobest \
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
        jq-devel \
        c-ares-devel && \
    /tmp/.rhtap/scripts/subscription-manager-bro.sh cleanup && \
    # We can do usual cleanup while we're here: remove packages that would trigger violations. \
    dnf -y --installroot=/mnt clean all && \
    rpm --root=/mnt --verbose -e --nodeps $(rpm --root=/mnt -qa 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*') && \
    rm -rf /mnt/var/cache/dnf /mnt/var/cache/yum

FROM scratch as builder

COPY --from=rpm-implanter-builder /mnt /

COPY . .

ARG BUILD_DIR
ARG SRC_ROOT_DIR=${BUILD_DIR}
ARG CMAKE_BUILD_DIR
# TODO(ROX-20240): CMAKE_BUILD_TYPE should probably not be Release for PR, normal branch builds
ARG CMAKE_BUILD_TYPE=Release
# Appends an argument to the driver download URL that is used for filtering alerts on missing kernels.
# TODO(ROX-20240): This needs to be true on PRs only.
ARG COLLECTOR_APPEND_CID=false
ARG USE_VALGRIND=false
ARG ADDRESS_SANITIZER=false
ARG TRACE_SINSP_EVENTS=false

WORKDIR ${BUILD_DIR}

RUN mkdir kernel-modules \
    && cp -a /builder builder \
    && cp -a /collector collector \
    && cp -a /falcosecurity-libs falcosecurity-libs \
    && cp -a /builder/third_party third_party \
    && cp -a /kernel-modules/MODULE_VERSION kernel-modules/MODULE_VERSION \
    && cp -a /CMakeLists.txt CMakeLists.txt

# WITH_RHEL_RPMS controls for dependency installation, ie if they were already installed as RPMs.
# Setting the value to empty will cause dependencies to be downloaded from repositories or accessed in submodules and compiled.
# TODO(ROX-20651): Set ENV WITH_RHEL_RPMS=true when RHEL RPMs can be installed to enable hermetic builds.
ENV WITH_RHEL_RPMS=true

# Build with gperftools (DISABLE_PROFILING=OFF) only for supported
# architectures, at the moment x86_64 only
RUN ./builder/install/install-dependencies.sh && \
    if [[ "$(uname -m)" == "x86_64" ]];   \
        then DISABLE_PROFILING="OFF";   \
        else DISABLE_PROFILING="ON";    \
    fi ; \
    cmake -S ${SRC_ROOT_DIR} -B ${CMAKE_BUILD_DIR} \
           -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
           -DDISABLE_PROFILING=${DISABLE_PROFILING} \
           -DCOLLECTOR_APPEND_CID=${COLLECTOR_APPEND_CID} \
           -DUSE_VALGRIND=${USE_VALGRIND} \
           -DADDRESS_SANITIZER=${ADDRESS_SANITIZER} \
           -DTRACE_SINSP_EVENTS=${TRACE_SINSP_EVENTS} && \
    cmake --build ${CMAKE_BUILD_DIR} --target all -- -j "${NPROCS:-2}" && \
    ctest -V --test-dir ${CMAKE_BUILD_DIR} && \
    strip -v --strip-unneeded "${CMAKE_BUILD_DIR}/collector/collector" && \
    if [[ -f "${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so" ]]; then \
        strip -v --strip-unneeded "${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so"; fi


# Application
FROM registry.access.redhat.com/ubi8/ubi-minimal:latest AS ubi-minimal
# The installer must be ubi (not minimal) and must be 8.9 or later since the earlier versions complain:
#  subscription-manager is disabled when running inside a container. Please refer to your host system for subscription management.
FROM ubi-normal AS rpm-implanter-app

COPY --from=ubi-minimal / /mnt
COPY ./.rhtap /tmp/.rhtap

# TODO(ROX-20234): use hermetic builds when installing/updating RPMs becomes hermetic.
RUN /tmp/.rhtap/scripts/subscription-manager-bro.sh register && \
    dnf -y --installroot=/mnt upgrade --nobest && \
    dnf -y --installroot=/mnt install --nobest \
      kmod \
      tbb \
      jq \
      c-ares && \
    /tmp/.rhtap/scripts/subscription-manager-bro.sh cleanup && \
    # We can do usual cleanup while we're here: remove packages that would trigger violations. \
    dnf -y --installroot=/mnt clean all && \
    rpm --root=/mnt --verbose -e --nodeps $(rpm --root=/mnt -qa 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*') && \
    rm -rf /mnt/var/cache/dnf /mnt/var/cache/yum

FROM scratch

# TODO(ROX-20236): configure injection of dynamic version value when it becomes possible.
ARG COLLECTOR_VERSION=0.0.1-todo

COPY --from=rpm-implanter-app /mnt /

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
    version=${COLLECTOR_VERSION}

ARG BUILD_DIR
ARG CMAKE_BUILD_DIR

ENV COLLECTOR_HOST_ROOT=/host

COPY --from=builder ${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so /usr/local/lib/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/collector /usr/local/bin/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/self-checks /usr/local/bin/
COPY --from=builder ${BUILD_DIR}/collector/container/scripts /

RUN mv /collector-wrapper.sh /usr/local/bin/ && \
    chmod 700 bootstrap.sh && \
    echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && \
    ldconfig && \
    mkdir /kernel-modules

COPY --from=builder ${BUILD_DIR}/kernel-modules/MODULE_VERSION /kernel-modules/MODULE_VERSION.txt

EXPOSE 8080 9090

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER
