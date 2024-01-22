ARG TEST_ROOT="/tests"

FROM golang:1.19 as builder

ARG TEST_ROOT

RUN mkdir -p "$TEST_ROOT"

COPY suites "$TEST_ROOT/suites/"
COPY integration_test.go "$TEST_ROOT"
COPY benchmark_test.go "$TEST_ROOT"
COPY go.* "$TEST_ROOT"

WORKDIR "$TEST_ROOT"

RUN CGO_ENABLED=0 GOOS=linux GOARCH=$TARGETARCH go test -tags bench -c -o collector-tests

FROM alpine:3.18

ARG TEST_ROOT

RUN apk add docker

COPY --from=builder $TEST_ROOT/collector-tests $TEST_ROOT/collector-tests
COPY images.yml "$TEST_ROOT"

WORKDIR "$TEST_ROOT"

ENTRYPOINT ["./collector-tests"]
