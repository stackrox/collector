FROM registry.access.redhat.com/ubi8/ubi-minimal:8.7

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

COPY container/scripts/collector-wrapper.sh /
COPY container/scripts/bootstrap.sh /
COPY container/THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY LICENSE-kernel-modules.txt /kernel-modules/LICENSE
COPY container/libs/libsinsp-wrapper.so /usr/local/lib/
COPY container/bin/collector /usr/local/bin/
COPY container/bin/self-checks /usr/local/bin/self-checks

COPY container/${BUILD_TYPE}/final-step.sh /

RUN ./final-step.sh && rm -f final-step.sh


EXPOSE 8080 9090

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER
