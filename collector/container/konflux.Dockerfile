ARG BUILD_DIR=/build
ARG CMAKE_BUILD_DIR=${BUILD_DIR}/cmake-build

# Builder
# TODO(ROX-20312): we can't pin image tag or digest because currently there's no mechanism to auto-update that.
# TODO(ROX-20651): use content sets instead of subscription manager for access to RHEL RPMs once available.
FROM registry.access.redhat.com/ubi8/ubi:latest AS ubi-normal
FROM registry.access.redhat.com/ubi8/ubi:latest AS rpm-implanter-builder

COPY --from=ubi-normal / /mnt
COPY ./.konflux /tmp/.konflux

# TODO(ROX-20234): use hermetic builds when installing/updating RPMs becomes hermetic.
RUN /tmp/.konflux/scripts/subscription-manager-bro.sh register /mnt && \
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
    /tmp/.konflux/scripts/subscription-manager-bro.sh cleanup && \
    dnf -y --installroot=/mnt clean all

FROM scratch as builder

COPY --from=rpm-implanter-builder /mnt /

ARG SOURCES_DIR=/staging

COPY . ${SOURCES_DIR}

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
    && cp -a ${SOURCES_DIR}/builder builder \
    && ln -s builder/third_party third_party \
    && cp -a ${SOURCES_DIR}/collector collector \
    && cp -a ${SOURCES_DIR}/falcosecurity-libs falcosecurity-libs \
    && cp -a ${SOURCES_DIR}/kernel-modules/MODULE_VERSION kernel-modules/MODULE_VERSION \
    && cp -a ${SOURCES_DIR}/CMakeLists.txt CMakeLists.txt

# WITH_RHEL_RPMS controls for dependency installation, ie if they were already installed as RPMs.
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
    strip -v --strip-unneeded "${CMAKE_BUILD_DIR}/collector/collector"

# 0.1.0 is a floating tag and it's used intentionally to pick up the most recent downstream drivers build without
# having to routinely and frequently bump tags here.
FROM brew.registry.redhat.io/rh-osbs/rhacs-drivers-build-rhel8:0.1.0 AS drivers-build

# TODO(ROX-20312): we can't pin image tag or digest because currently there's no mechanism to auto-update that.
FROM registry.access.redhat.com/ubi8/ubi-minimal:latest AS unpacker

RUN microdnf install -y unzip findutils
WORKDIR /staging

COPY staging/support-pkg.zip /staging/
COPY kernel-modules/MODULE_VERSION MODULE_VERSION.txt
RUN mkdir -p "/staging/kernel-modules/$(cat MODULE_VERSION.txt)"

# First, unpack upstream support package, only on x86_64
RUN if [[ "$(uname -m)" == x86_64 ]]; then unzip support-pkg.zip ; fi
# Fail build if there were no drivers in the support package matching the module version.
RUN if [[ "$(uname -m)" == x86_64 && "$(ls -A /staging/kernel-modules/$(cat MODULE_VERSION.txt))" == "" ]] ; then \
      >&2 echo "Did not find any kernel drivers for the module version $(cat MODULE_VERSION.txt) in the support package"; \
      exit 1; \
    fi

# Next, import modules from downstream build, which take priority over upstream, on non-x86 architectures
COPY --from=drivers-build /kernel-modules /staging/downstream
RUN if [[ "$(uname -m)" != x86_64 ]]; then \
      cp -r /staging/downstream/. /staging/kernel-modules/ ; \
    fi

# Create destination for drivers.
RUN mkdir /kernel-modules
# Move files for the current version to /kernel-modules
RUN find "/staging/kernel-modules/$(cat MODULE_VERSION.txt)/" -type f -exec mv -t /kernel-modules {} +
# Fail the build if at the end there were no drivers matching the module version.
RUN if [[ "$(ls -A /kernel-modules)" == "" ]]; then \
        >&2 echo "Did not find any kernel drivers for the module version $(cat MODULE_VERSION.txt)."; \
        exit 1; \
    fi

# Application
FROM registry.access.redhat.com/ubi8/ubi-minimal:latest AS ubi-minimal
FROM ubi-normal AS rpm-implanter-app

COPY --from=ubi-minimal / /mnt
COPY ./.konflux /tmp/.konflux

# TODO(ROX-20234): use hermetic builds when installing/updating RPMs becomes hermetic.
RUN /tmp/.konflux/scripts/subscription-manager-bro.sh register /mnt && \
    dnf -y --installroot=/mnt upgrade --nobest && \
    dnf -y --installroot=/mnt install --nobest \
      kmod \
      tbb \
      jq \
      c-ares && \
    /tmp/.konflux/scripts/subscription-manager-bro.sh cleanup && \
    # We can do usual cleanup while we're here: remove packages that would trigger violations. \
    dnf -y --installroot=/mnt clean all && \
    rpm --root=/mnt --verbose -e --nodeps $(rpm --root=/mnt -qa 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*') && \
    rm -rf /mnt/var/cache/dnf /mnt/var/cache/yum

FROM scratch

COPY --from=rpm-implanter-app /mnt /

# TODO(ROX-20236): configure injection of dynamic version value when it becomes possible.
ARG COLLECTOR_VERSION=0.0.1-todo

WORKDIR /

LABEL \
    com.redhat.component="rhacs-collector-container" \
    com.redhat.license_terms="https://www.redhat.com/agreements" \
    description="This image supports runtime data collection in the StackRox Kubernetes Security Platform" \
    distribution-scope="public" \
    io.k8s.description="This image supports runtime data collection in the StackRox Kubernetes Security Platform" \
    io.k8s.display-name="collector" \
    io.openshift.tags="rhacs,collector,stackrox" \
    maintainer="Red Hat, Inc." \
    name="rhacs-collector-rhel8" \
    # TODO(ROX-20236): release label is required by EC, figure what to put in the release version on rebuilds.
    release="0" \
    source-location="https://github.com/stackrox/collector" \
    summary="Runtime data collection for the StackRox Kubernetes Security Platform" \
    url="https://catalog.redhat.com/software/container-stacks/detail/60eefc88ee05ae7c5b8f041c" \
    version=${COLLECTOR_VERSION} \
    vendor="Red Hat, Inc."

ARG BUILD_DIR
ARG CMAKE_BUILD_DIR

ENV COLLECTOR_HOST_ROOT=/host

COPY --from=unpacker /kernel-modules /kernel-modules
COPY kernel-modules/MODULE_VERSION /kernel-modules/MODULE_VERSION.txt
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/collector /usr/local/bin/
COPY --from=builder ${CMAKE_BUILD_DIR}/collector/self-checks /usr/local/bin/
COPY --from=builder ${BUILD_DIR}/collector/container/scripts /

RUN mv /collector-wrapper.sh /usr/local/bin/ && \
    chmod 700 bootstrap.sh && \
    echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && \
    ldconfig

EXPOSE 8080 9090

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER
