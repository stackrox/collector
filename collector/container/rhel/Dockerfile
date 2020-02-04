ARG BASE_REGISTRY=registry.access.redhat.com
ARG BASE_IMAGE=ubi7/ubi
ARG BASE_TAG=7.7

FROM ${BASE_REGISTRY}/${BASE_IMAGE}:${BASE_TAG}

ARG collector_version=xxx
ARG module_version=xxx

# ADD DSOP LABELS
LABEL maintainer="StackRox <support@stackrox.com>"
LABEL io.stackrox.collector.module-version="${module_version}"
LABEL io.stackrox.collector.version="${collector_version}"

ENV COLLECTOR_VERSION=${collector_version}
ENV MODULE_VERSION=${module_version}
ENV COLLECTOR_HOST_ROOT=/host

RUN yum update -y --disableplugin=subscription-manager && \
    rpm -e --nodeps yum yum-utils yum-plugin-ovl subscription-manager rpm rpm-libs rpm-build-libs rpm-python subscription-manager-rhsm

ADD bundle.tar.gz /

RUN echo "${module_version}" >/kernel-modules/MODULE_VERSION.txt

EXPOSE 8080 9090

WORKDIR /

RUN chown root.root /bootstrap.sh
RUN chmod 700 /bootstrap.sh

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER

HEALTHCHECK CMD curl --fail http://127.0.0.1:8080/ready || exit 1
