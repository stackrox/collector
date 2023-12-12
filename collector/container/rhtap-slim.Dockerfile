ARG BUILD_DIR=/build
ARG CMAKE_BUILD_DIR=${BUILD_DIR}/cmake-build

# TODO(ROX-20312): we can't pin image tag or digest because currently there's no mechanism to auto-update that.
# TODO(ROX-20651): Use RHEL/ubi base image when entitlement is solved.
# RPMs requiring entitlement: bpftool, cmake-3.18.2-9.el8, elfutils-libelf-devel, tbb-devel, c-ares-devel, jq-devel
# FROM registry.access.redhat.com/ubi8/ubi:latest AS builder
FROM quay.io/centos/centos:stream8 AS builder

COPY . .

# TODO(ROX-20234): use hermetic builds when installing/updating RPMs becomes hermetic.
RUN dnf upgrade -y --nobest \
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
# ENV WITH_RHEL_RPMS=true

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


FROM registry.access.redhat.com/ubi8/ubi-minimal:latest

ARG BUILD_DIR
ARG CMAKE_BUILD_DIR

# TODO(ROX-20236): configure injection of dynamic version value when it becomes possible.
ARG COLLECTOR_VERSION=0.0.1-todo
ARG MODULE_VERSION=0.0.1-todo

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
    version=${COLLECTOR_VERSION}

WORKDIR /

COPY collector/container/rhel/install.sh /
RUN ./install.sh && rm -f install.sh

COPY collector/container/scripts/collector-wrapper.sh /usr/local/bin/
COPY collector/container/scripts/bootstrap.sh /
COPY collector/LICENSE-kernel-modules.txt /kernel-modules/LICENSE
COPY kernel-modules/MODULE_VERSION /kernel-modules/MODULE_VERSION.txt
COPY --from=builder /THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY --from=builder /collector/NOTICE-sysdig.txt /THIRD_PARTY_NOTICES/sysdig
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/collector /usr/local/bin/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/self-checks /usr/local/bin/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so /usr/local/lib/

RUN echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && \
    ldconfig && \
    chmod 700 bootstrap.sh

EXPOSE 8080 9090

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER
