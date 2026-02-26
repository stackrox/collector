FROM registry.access.redhat.com/ubi10/ubi-minimal:latest

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

COPY container/${BUILD_TYPE}/install.sh /
RUN ./install.sh && rm -f install.sh

# Uncomment this line to enable generation of core for collector
# RUN echo '/core/core.%e.%p.%t' > /proc/sys/kernel/core_pattern

COPY container/THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY kernel-modules /kernel-modules
COPY container/bin/collector /usr/local/bin/
COPY container/bin/self-checks /usr/local/bin/self-checks
COPY container/bin/libcontainer.so /usr/local/lib64/libcontainer.so
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
