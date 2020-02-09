ARG BASE_REGISTRY=registry.access.redhat.com
ARG BASE_IMAGE=ubi7/ubi
ARG BASE_TAG=7.7

FROM ${BASE_REGISTRY}/${BASE_IMAGE}:${BASE_TAG}

ARG collector_version=xxx
ARG module_version=xxx

LABEL maintainer="StackRox <support@stackrox.com>"
LABEL io.stackrox.collector.module-version="${module_version}"
LABEL io.stackrox.collector.version="${collector_version}"

ENV COLLECTOR_VERSION=${collector_version}
ENV MODULE_VERSION=${module_version}
ENV COLLECTOR_HOST_ROOT=/host

RUN yum update -y --nogpgcheck --disableplugin=subscription-manager
RUN rpm -e --nodeps \
      rpm \
      rpm-build-libs \
      rpm-libs \
      rpm-python \
      subscription-manager \
      subscription-manager-rhsm \
      yum \
      yum-plugin-ovl \
      yum-utils \
    ;

ADD bundle.tar.gz /

RUN echo "${module_version}" >/kernel-modules/MODULE_VERSION.txt

EXPOSE 8080 9090

WORKDIR /

RUN chmod 700 /bootstrap.sh

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER

HEALTHCHECK CMD curl --fail http://127.0.0.1:8080/ready || exit 1
