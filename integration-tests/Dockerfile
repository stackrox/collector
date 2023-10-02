FROM quay.io/stackrox-io/apollo-ci:collector-0.3.60

ARG TEST_ROOT="/tests/"
ARG QA_TAG
ARG COLLECTOR_IMAGE

ENV COLLECTOR_QA_TAG="$QA_TAG"
ENV COLLECTOR_IMAGE="$COLLECTOR_IMAGE"

RUN mkdir -p "$TEST_ROOT"

COPY suites "$TEST_ROOT/suites/"
COPY integration_test.go "$TEST_ROOT"
COPY benchmark_test.go "$TEST_ROOT"
COPY images.yml "$TEST_ROOT"
COPY go.* "$TEST_ROOT"

WORKDIR "$TEST_ROOT"

ENTRYPOINT ["go", "test"]
