FROM registry.access.redhat.com/ubi10/ubi-micro:latest AS ubi-micro-base

FROM registry.access.redhat.com/ubi10/ubi:latest AS package_installer

# Copy ubi-micro base to /out to preserve its rpmdb
COPY --from=ubi-micro-base / /out/

# Install packages directly to /out/ using --installroot
RUN dnf install -y \
    --installroot=/out/ \
    --releasever=10 \
    --setopt=install_weak_deps=False \
    --nodocs \
    ca-certificates \
    curl-minimal \
    elfutils-libelf \
    gzip \
    less \
    libcap-ng \
    libstdc++ \
    libuuid \
    openssl \
    tar \
    tbb && \
    dnf clean all --installroot=/out/ && \
    rm -rf /out/var/cache/dnf /out/var/cache/yum

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
COPY container/bin/collector /usr/local/bin/
COPY container/bin/self-checks /usr/local/bin/self-checks
COPY container/status-check.sh /usr/local/bin/status-check.sh

EXPOSE 8080 9090

HEALTHCHECK	\
	# health checks within the first 5s are not counted as failure
	--start-period=5s \
	# perform health check every 5s
	--interval=5s \
	# the command uses /ready API
	CMD /usr/local/bin/status-check.sh

ENTRYPOINT ["collector"]
