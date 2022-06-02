ARG BASE_REGISTRY=registry.access.redhat.com
ARG BASE_IMAGE=ubi8/ubi-minimal
ARG BASE_TAG=8.6
FROM ${BASE_REGISTRY}/${BASE_IMAGE}:${BASE_TAG} AS extracted_bundle

COPY bundle.tar.gz /
COPY extract-bundle.sh /bundle/
WORKDIR /bundle
RUN ./extract-bundle.sh

FROM ${BASE_REGISTRY}/${BASE_IMAGE}:${BASE_TAG}

ARG collector_version=xxx
ARG module_version=xxx

LABEL name="collector" \
      vendor="StackRox" \
      maintainer="support@stackrox.com" \
      summary="Runtime data collection for the StackRox Kubernetes Security Platform" \
      description="This image supports runtime data collection in the StackRox Kubernetes Security Platform." \
      io.stackrox.collector.module-version="${module_version}" \
      io.stackrox.collector.version="${collector_version}"

ENV COLLECTOR_VERSION=${collector_version} \
    MODULE_VERSION=${module_version} \
    COLLECTOR_HOST_ROOT=/host

WORKDIR /

COPY scripts /

COPY --from=extracted_bundle /bundle/THIRD_PARTY_NOTICES/ /THIRD_PARTY_NOTICES/
COPY --from=extracted_bundle /bundle/kernel-modules/ /kernel-modules/
COPY --from=extracted_bundle /bundle/usr/local/lib/libsinsp-wrapper.so /usr/local/lib/
COPY --from=extracted_bundle /bundle/usr/local/bin/collector /usr/local/bin/

COPY final-step.sh /

RUN ./final-step.sh && rm -f final-step.sh


EXPOSE 8080 9090

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER
