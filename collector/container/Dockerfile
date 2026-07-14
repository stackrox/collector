# UBI-micro: required for Red Hat container certification. The smallest UBI
# variant — no package manager, keeping the final image minimal.
FROM registry.access.redhat.com/ubi10/ubi-micro:latest AS ubi-micro-base

# ubi-micro has no dnf, so we use the full UBI image to install packages into
# an --installroot overlay. The final image gets the packages without dnf.
FROM registry.access.redhat.com/ubi10/ubi:latest AS package_installer

# Copy ubi-micro base to /out to preserve its rpmdb
COPY --from=ubi-micro-base / /out/

# Install runtime dependencies into /out/ using --installroot.
# elfutils-libelf: BPF ELF parsing; tbb: sinsp runtime dep; curl-minimal:
# healthcheck; libcap-ng: capability ops; ca-certificates: TLS to Sensor.
RUN dnf install -y \
    --installroot=/out/ \
    --releasever=10 \
    --setopt=install_weak_deps=False \
    --nodocs \
    ca-certificates curl-minimal elfutils-libelf gzip less libcap-ng libstdc++ libuuid openssl tar tbb && \
    dnf clean all --installroot=/out/ && \
    rm -rf /out/var/cache/dnf /out/var/cache/yum

# Final image is based on ubi-micro (not package_installer) to exclude dnf
# and the full UBI toolchain — only the /out/ overlay is merged in.
FROM ubi-micro-base

ARG COLLECTOR_VERSION

ENV COLLECTOR_HOST_ROOT=/host

LABEL name="collector" \
      vendor="StackRox" \
      maintainer="support@stackrox.com" \
      summary="Runtime data collection for the StackRox Kubernetes Security Platform" \
      description="This image supports runtime data collection in the StackRox Kubernetes Security Platform." \
      io.stackrox.collector.version="${COLLECTOR_VERSION}"

WORKDIR /

COPY --from=package_installer /out/ /

# Uncomment this line to enable generation of core for collector
# RUN echo '/core/core.%e.%p.%t' > /proc/sys/kernel/core_pattern

COPY container/THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY kernel-modules /kernel-modules
# self-checks is a separate binary for BPF driver validation at startup,
# kept independent from the full collector dependency tree.
COPY container/bin/collector /usr/local/bin/
COPY container/bin/self-checks /usr/local/bin/self-checks
COPY container/status-check.sh /usr/local/bin/status-check.sh

# 8080: civetweb introspection (/ready, /state). 9090: Prometheus metrics.
EXPOSE 8080 9090

# Docker-native healthcheck for non-K8s environments and local dev/debugging.
# 5s start-period avoids false failures while the BPF driver loads.
HEALTHCHECK	\
	# health checks within the first 5s are not counted as failure
	--start-period=5s \
	# perform health check every 5s
	--interval=5s \
	# the command uses /ready API
	CMD /usr/local/bin/status-check.sh

# Exec form ensures collector runs as PID 1, receiving signals directly for
# graceful shutdown of the BPF probe and gRPC connection.
ENTRYPOINT ["collector"]
