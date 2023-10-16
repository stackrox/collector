ARG REMOTE_SOURCES_DIR=.
ARG BUILD_DIR=/build
ARG SRC_ROOT_DIR=${BUILD_DIR}
ARG CMAKE_BUILD_DIR=${SRC_ROOT_DIR}/cmake-build
ARG USE_VALGRIND=false
ARG ADDRESS_SANITIZER=false

#@follow_tag(registry.access.redhat.com/ubi8/ubi:latest)
FROM registry.access.redhat.com/ubi8/ubi-minimal:latest AS builder

COPY . ${REMOTE_SOURCES_DIR}

# hadolint ignore=DL3041
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
        jq-devel \
        c-ares-devel && \
    dnf clean all

ARG BUILD_DIR
ENV BUILD_DIR=${BUILD_DIR}

ARG SRC_ROOT_DIR
ENV SRC_ROOT_DIR=${SRC_ROOT_DIR}

ARG CMAKE_BUILD_DIR
ENV CMAKE_BUILD_DIR=${CMAKE_BUILD_DIR}

# This won't be true for all builds (eg. not for PR, master, ...)
ARG CMAKE_BUILD_TYPE=Release
ENV CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}

ARG COLLECTOR_APPEND_CID=false
ENV COLLECTOR_APPEND_CID=${COLLECTOR_APPEND_CID}

RUN mkdir -p ${BUILD_DIR}
WORKDIR ${BUILD_DIR}
RUN cp -a ${REMOTE_SOURCES_DIR}/collector/app/builder builder
RUN cp -a ${REMOTE_SOURCES_DIR}/collector/app/collector collector
RUN cp -a ${REMOTE_SOURCES_DIR}/collector/app/falcosecurity-libs falcosecurity-libs
RUN cp -a ${REMOTE_SOURCES_DIR}/collector/app/third_party third_party
RUN mkdir kernel-modules
RUN cp -a ${REMOTE_SOURCES_DIR}/collector/app/kernel-modules/MODULE_VERSION kernel-modules/MODULE_VERSION
RUN cp -a ${REMOTE_SOURCES_DIR}/collector/app/CMakeLists.txt CMakeLists.txt

# ENV DISABLE_PROFILING=true
# # TODO: Remove this variable once upstream is fully merged
# ENV WITH_RHEL8_RPMS=true
# ENV WITH_RHEL_RPMS=true
# RUN ./builder/install/install-dependencies.sh && \
#     ./builder/build/build-collector.sh && \
#     "${CMAKE_BUILD_DIR}/collector/runUnitTests"

# # Do NOT use follow_tag here, as we do not need or want collector to be rebuilt
# # with each drivers build (which may become very frequent)
# FROM registry-proxy.engineering.redhat.com/rh-osbs/rhacs-drivers-build-rhel8:0.1.0 AS drivers-build

# #@follow_tag(registry.access.redhat.com/ubi8/ubi-minimal:latest)
# FROM registry.access.redhat.com/ubi8/ubi-minimal:8.8-1072.1696517598 AS unpacker
# ARG BUILD_DIR

# RUN microdnf install -y unzip findutils
# WORKDIR /staging

# COPY support-pkg.zip /staging/
# COPY --from=builder ${BUILD_DIR}/kernel-modules/MODULE_VERSION MODULE_VERSION.txt
# # Creating this directory ensures the scratch build with dummy support-pkg.zip will not fail.
# RUN mkdir -p "/staging/kernel-modules/$(cat MODULE_VERSION.txt)"
# # First, unpack upstream support package, only on x86_64
# RUN if [[ "$(uname -m)" == x86_64 ]]; then unzip support-pkg.zip ; fi
# # Fail non-scratch build if there were no drivers matching the module version.
# RUN if [[ "$(uname -m)" == x86_64 && "$(ls -A /staging/kernel-modules/$(cat MODULE_VERSION.txt))" == "" && "$(unzip -Z1 support-pkg.zip)" != "dummy-support-pkg" ]] ; then \
#       >&2 echo "Did not find any kernel drivers for the module version $(cat MODULE_VERSION.txt) in the support package"; \
#       exit 1; \
#     fi

# # Next, import modules from downstream build, which take priority over upstream, on non-x86 architectures
# # TODO(ROX-13563): find a way to not have to separately pull in the support package and downstream-built drivers.
# COPY --from=drivers-build /kernel-modules /staging/downstream
# RUN if [[ "$(uname -m)" != x86_64 ]]; then \
#       cp -r /staging/downstream/. /staging/kernel-modules/ ; \
#     fi

# # Create destination for drivers.
# RUN mkdir /kernel-modules
# # Move files for the current version to /kernel-modules
# RUN find "/staging/kernel-modules/$(cat MODULE_VERSION.txt)/" -type f -exec mv -t /kernel-modules {} +
# # Fail the build if at the end there were no drivers matching the module version.
# RUN if [[ "$(ls -A /kernel-modules)" == "" && \
#         !("$(uname -m)" == x86_64 && "$(unzip -Z1 support-pkg.zip)" == "dummy-support-pkg") ]]; then \
#         >&2 echo "Did not find any kernel drivers for the module version $(cat MODULE_VERSION.txt)."; \
#         exit 1; \
#     fi

# #@follow_tag(registry.access.redhat.com/ubi8/ubi-minimal:latest)
# FROM registry.access.redhat.com/ubi8/ubi-minimal:8.8-1072.1696517598

# WORKDIR /

# LABEL \
#     com.redhat.component="rhacs-collector-container" \
#     name="rhacs-collector-rhel8" \
#     maintainer="Red Hat, Inc." \
#     version="${CI_VERSION}" \
#     "git-commit:stackrox/collector"="${CI_COLLECTOR_UPSTREAM_COMMIT}" \
#     "git-branch:stackrox/collector"="${CI_COLLECTOR_UPSTREAM_BRANCH}" \
#     "git-tag:stackrox/collector"="${CI_COLLECTOR_UPSTREAM_TAG}"


# ARG COLLECTOR_VERSION
# LABEL collector_version="${COLLECTOR_VERSION}"

# ARG BUILD_DIR
# ARG CMAKE_BUILD_DIR

# ENV COLLECTOR_HOST_ROOT=/host

# COPY --from=builder ${CMAKE_BUILD_DIR}/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so /usr/local/lib/
# COPY --from=builder ${CMAKE_BUILD_DIR}/collector/collector /usr/local/bin/
# COPY --from=builder ${CMAKE_BUILD_DIR}/collector/self-checks /usr/local/bin/
# COPY --from=builder ${BUILD_DIR}/collector/container/scripts /

# RUN mv /collector-wrapper.sh /usr/local/bin/ && \
#     chmod 700 bootstrap.sh && \
#     echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && \
#     ldconfig && \
#     mkdir /kernel-modules && \
#     microdnf upgrade -y --nobest && \
#     microdnf install -y \
#       kmod \
#       tbb \
#       jq \
#       c-ares && \
#     microdnf clean all && \
#     rpm --verbose -e --nodeps $(rpm -qa 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*') && \
#     rm -rf /var/cache/dnf /var/cache/yum

# COPY --from=unpacker /kernel-modules /kernel-modules
# COPY --from=builder ${BUILD_DIR}/kernel-modules/MODULE_VERSION /kernel-modules/MODULE_VERSION.txt

# EXPOSE 8080 9090

# ENTRYPOINT ["/bootstrap.sh"]

# # hadolint ignore=DL3025
# CMD collector-wrapper.sh \
#     --collector-config=$COLLECTOR_CONFIG \
#     --collection-method=$COLLECTION_METHOD \
#     --grpc-server=$GRPC_SERVER
