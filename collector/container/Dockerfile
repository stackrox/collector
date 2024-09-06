ARG BUILDER_TAG=master
FROM quay.io/stackrox-io/collector-builder:${BUILDER_TAG} AS builder

ARG CMAKE_BUILD_TYPE=Release
ARG USE_VALGRIND=false
ARG ADDRESS_SANITIZER=false
ARG TRACE_SINSP_EVENTS=false
ARG COLLECTOR_VERSION=0.0.0
ARG BPF_DEBUG_MODE=false

COPY . /collector-src
WORKDIR /collector-src

RUN --mount=type=cache,target=/collector-src/build \
    if [[ "$(uname -m)" == "x86_64" ]];   \
        then DISABLE_PROFILING="OFF";   \
        else DISABLE_PROFILING="ON";    \
    fi ; \
    cmake -S . -B build/ \
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
        -DDISABLE_PROFILING=${DISABLE_PROFILING} \
        -DUSE_VALGRIND=${USE_VALGRIND} \
        -DADDRESS_SANITIZER=${ADDRESS_SANITIZER} \
        -DCOLLECTOR_VERSION=${COLLECTOR_VERSION} \
        -DBPF_DEBUG_MODE=${BPF_DEBUG_MODE} \
        -DTRACE_SINSP_EVENTS=${TRACE_SINSP_EVENTS} && \
    cmake --build build/ -- -j$(nproc) && \
    ctest -V --test-dir build && \
    strip -v --strip-unneeded build/collector/collector && \
    # podman does not bake cache mounts into the final image, so
    # we need to move the required binaries out of it.
    cp build/collector/collector /tmp/ && \
    cp build/collector/self-checks /tmp/

FROM registry.access.redhat.com/ubi9/ubi-minimal:9.3

ARG BUILD_TYPE=rhel
ARG ROOT_DIR=.
ARG COLLECTOR_VERSION

ENV ROOT_DIR=$ROOT_DIR
ENV COLLECTOR_HOST_ROOT=/host

LABEL name="collector" \
      vendor="StackRox" \
      maintainer="support@stackrox.com" \
      summary="Runtime data collection for the StackRox Kubernetes Security Platform" \
      description="This image supports runtime data collection in the StackRox Kubernetes Security Platform." \
      io.stackrox.collector.version="${COLLECTOR_VERSION}"

WORKDIR /

COPY collector/container/${BUILD_TYPE}/install.sh /
RUN ./install.sh && rm -f install.sh

# Uncomment this line to enable generation of core for collector
# RUN echo '/core/core.%e.%p.%t' > /proc/sys/kernel/core_pattern

COPY --from=builder /THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY collector/kernel-modules /kernel-modules
COPY --from=builder /tmp/collector /usr/local/bin/
COPY --from=builder /tmp/self-checks /usr/local/bin/self-checks
COPY collector/container/status-check.sh /usr/local/bin/status-check.sh

EXPOSE 8080 9090

HEALTHCHECK	\
	# health checks within the first 5s are not counted as failure
	--start-period=5s \
	# perform health check every 5s
	--interval=5s \
	# the command uses /ready API
	CMD /usr/local/bin/status-check.sh

ENTRYPOINT ["collector"]
