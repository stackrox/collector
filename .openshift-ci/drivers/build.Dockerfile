FROM replaced-by-osci:builder-base AS builder

ARG SHARDS_DIR
ENV SHARDS_DIR=${SHARDS_DIR}

ENV DOCKERIZED=1
ENV OSCI_RUN=1

COPY --from=replaced-by-osci:pre-build /kobuild-tmp/versions-src /kobuild-tmp/versions-src
COPY --from=replaced-by-osci:pre-build /tasks/ /tasks/

RUN /scripts/run-builders.sh

FROM registry.fedoraproject.org/fedora:36

COPY --from=builder /built-drivers /built-drivers
COPY --from=builder /FAILURES /FAILURES
