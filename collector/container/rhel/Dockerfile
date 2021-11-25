ARG BASE_REGISTRY=registry.access.redhat.com
ARG BASE_IMAGE=ubi8/ubi
ARG BASE_TAG=8.5

FROM ${BASE_REGISTRY}/${BASE_IMAGE}:${BASE_TAG} AS valgrind_build

ARG USE_VALGRIND=false

RUN if [ $USE_VALGRIND == "true" ]; then \
      echo "Installing valgrind"; \
      yum update -y && \
      yum install -y --enablerepo=ubi-8-appstream --enablerepo=ubi-8-baseos --enablerepo=ubi-8-codeready-builder --disableplugin=subscription-manager perl --nobest && \
      yum install -y wget bzip2 gcc make && \
      wget https://sourceware.org/pub/valgrind/valgrind-3.17.0.tar.bz2 && \
      tar xjf valgrind-3.17.0.tar.bz2 && \
      cd valgrind-3.17.0 && \
      mkdir bin && \
      mkdir install && \
      ./configure MAKE="gmake" --disable-dependency-tracking --prefix=/valgrind-3.17.0/install && \
      make && \
      make install; \
    else \
      echo "Not installing valgrind" && \
      mkdir -p /valgrind-3.17.0/install/bin && \
      mkdir -p /valgrind-3.17.0/include; \
    fi

FROM ${BASE_REGISTRY}/${BASE_IMAGE}:${BASE_TAG} AS extracted_bundle

COPY bundle.tar.gz /
WORKDIR /bundle
RUN tar -xzf /bundle.tar.gz

FROM ${BASE_REGISTRY}/${BASE_IMAGE}:${BASE_TAG}

ARG collector_version=xxx
ARG module_version=xxx
ARG USE_VALGRIND=false
ARG ADDRESS_SANITIZER=false

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
COPY --from=valgrind_build /valgrind-3.17.0/install /valgrind-3.17.0/install
COPY --from=valgrind_build /valgrind-3.17.0/include /valgrind-3.17.0/include

COPY final-step.sh /

# Create the module directory, for downloading kernel objects
RUN mkdir -p /module

RUN ./final-step.sh && rm -f final-step.sh


EXPOSE 8080 9090

ENTRYPOINT ["/usr/local/bin/collector-wrapper.sh", "--collector-config=$COLLECTOR_CONFIG", "--collection-method=$COLLECTION_METHOD", "--grpc-server=$GRPC_SERVER"]

HEALTHCHECK CMD curl --fail http://127.0.0.1:8080/ready || exit 1
