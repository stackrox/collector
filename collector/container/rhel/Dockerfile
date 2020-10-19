ARG BASE_REGISTRY=registry.access.redhat.com
ARG BASE_IMAGE=ubi8/ubi
ARG BASE_TAG=8.2

FROM ${BASE_REGISTRY}/${BASE_IMAGE}:${BASE_TAG}

ARG collector_version=xxx
ARG module_version=xxx

LABEL name="collector-rhel" \
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
COPY bundle.tar.gz /

RUN mv collector-wrapper.sh /usr/local/bin/ && \
    chmod 700 bootstrap.sh && \
    tar -zxf bundle.tar.gz ./COPYING.txt && \
    tar -zxf bundle.tar.gz ./kernel-modules/ && \
    tar -zxf bundle.tar.gz ./usr/local/lib/libsinsp-wrapper.so && \
    tar -zxf bundle.tar.gz ./usr/local/bin/collector && \
    rm -f bundle.tar.gz && \
    dnf upgrade -y && \
    dnf install -y kmod && \
    dnf clean all && \
    rm -rf /var/cache/dnf && \
    # (Optional) Remove line below to keep package management utilities
    rpm -e --nodeps rpm rpm-build-libs rpm-libs python3-rpm subscription-manager python3-subscription-manager-rhsm yum $(rpm -qa *dnf*) python3-hawkey && \
    echo "${MODULE_VERSION}" > /kernel-modules/MODULE_VERSION.txt


EXPOSE 8080 9090

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER

HEALTHCHECK CMD curl --fail http://127.0.0.1:8080/ready || exit 1
