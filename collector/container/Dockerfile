FROM registry.access.redhat.com/ubi9/ubi-minimal:9.2

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
COPY container/libs/libsinsp-wrapper.so /usr/local/lib/
COPY container/bin/collector /usr/local/bin/
COPY container/bin/conntracker /usr/local/bin/conntracker
COPY container/bin/self-checks /usr/local/bin/self-checks
COPY container/bin/AFLplusplus /usr/local/bin/AFLplusplus

RUN echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && \
    ldconfig && \
    echo "${MODULE_VERSION}" > /kernel-modules/MODULE_VERSION.txt && \
    chmod 700 bootstrap.sh

