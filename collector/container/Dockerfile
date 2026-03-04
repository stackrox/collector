# Stage 1: Package installer base
FROM registry.access.redhat.com/ubi10/ubi:latest@sha256:f573194e8e5231f1c9340c497e1f8d9aa9dbb42b2849e60341e34f50eec9477e AS package_installer

# Stage 2: ubi-micro base (needed for copying to /out to preserve rpmdb)
FROM registry.access.redhat.com/ubi10/ubi-micro:latest@sha256:551f8ee81be3dbabd45a9c197f3724b9724c1edb05d68d10bfe85a5c9e46a458 AS ubi-micro-base

# Stage 3: Install packages on top of ubi-micro base
FROM package_installer AS package_installer_final

# Copy ubi-micro base to /out to preserve its rpmdb
COPY --from=ubi-micro-base / /out/

# Install packages directly to /out/ using --installroot
RUN dnf install -y \
    --installroot=/out/ \
    --releasever=10 \
    --setopt=install_weak_deps=False \
    --nodocs \
    bash coreutils curl grep gawk elfutils-libelf ca-certificates openssl && \
    dnf clean all --installroot=/out/ && \
    rm -rf /out/var/cache/*

# Stage 4: Final runtime image
FROM ubi-micro-base

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

# Copy installed packages from package_installer
COPY --from=package_installer_final /out/ /

# Uncomment this line to enable generation of core for collector
# RUN echo '/core/core.%e.%p.%t' > /proc/sys/kernel/core_pattern

COPY container/THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY kernel-modules /kernel-modules
COPY container/bin/collector container/bin/self-checks container/status-check.sh /usr/local/bin/

EXPOSE 8080 9090

HEALTHCHECK \
	# health checks within the first 5s are not counted as failure
	--start-period=5s \
	# perform health check every 5s
	--interval=5s \
	# the command uses /ready API
	CMD /usr/local/bin/status-check.sh

ENTRYPOINT ["collector"]
