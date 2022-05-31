FROM builder-base:latest AS builder

ARG SHARDS_DIR
ENV SHARDS_DIR=${SHARDS_DIR}

ENV DOCKERIZED=1
ENV OSCI_RUN=1

COPY --from=pre-build:latest /kobuild-tmp/versions-src /kobuild-tmp/versions-src
COPY --from=pre-build:latest /tasks/ /tasks/

RUN /scripts/run-builders.sh

FROM registry.fedoraproject.org/fedora:36

COPY --from=builder /kernel-modules /kernel-modules
COPY --from=builder /FAILURES /FAILURES
