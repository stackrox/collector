FROM alpine:3.18

ARG TEST_ROOT="/tests"

RUN apk add docker && \
    mkdir -p $TEST_ROOT

WORKDIR "$TEST_ROOT"

COPY bin/$TARGETARCH/collector-tests /usr/local/bin

COPY images.yml .

ENTRYPOINT ["collector-tests"]
