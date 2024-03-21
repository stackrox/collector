FROM registry.access.redhat.com/ubi9/ubi-minimal:9.3

ARG BUILD_TYPE=rhel
ARG ROOT_DIR=.
ARG COLLECTOR_VERSION
ARG MODULE_VERSION

ENV ROOT_DIR=$ROOT_DIR
ENV COLLECTOR_VERSION="${COLLECTOR_VERSION}"
ENV MODULE_VERSION="${MODULE_VERSION}"
ENV COLLECTOR_HOST_ROOT=/host

LABEL name="collector" \
      vendor="StackRox" \
      maintainer="support@stackrox.com" \
      summary="Runtime data collection for the StackRox Kubernetes Security Platform" \
      description="This image supports runtime data collection in the StackRox Kubernetes Security Platform." \
      io.stackrox.collector.module-version="${MODULE_VERSION}" \
      io.stackrox.collector.version="${COLLECTOR_VERSION}"

WORKDIR /

COPY container/${BUILD_TYPE}/install.sh /
RUN ./install.sh && rm -f install.sh

COPY container/scripts/collector-wrapper.sh /usr/local/bin
COPY container/scripts/bootstrap.sh /
COPY container/THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY LICENSE-kernel-modules.txt /kernel-modules/LICENSE
COPY container/bin/collector /usr/local/bin/
COPY container/bin/self-checks /usr/local/bin/self-checks
COPY container/status-check.sh /usr/local/bin/status-check.sh

RUN echo "${MODULE_VERSION}" > /kernel-modules/MODULE_VERSION.txt && \
    chmod 700 bootstrap.sh

EXPOSE 8080 9090

HEALTHCHECK	\
	# health checks within the first 5s are not counted as failure
	--start-period=5s \
	# perform health check every 5s
	--interval=5s \
	# the command uses /ready API
	CMD /usr/local/bin/status-check.sh

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER
