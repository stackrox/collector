ARG TEST_ROOT="/tests"

FROM golang:1.23 as builder

ARG TEST_ROOT

ENV GOCACHE=/root/.cache/go-build

RUN mkdir -p "$TEST_ROOT"
WORKDIR "$TEST_ROOT"

# Cache dependencies
COPY go.* "$TEST_ROOT"
RUN go mod download

COPY suites "$TEST_ROOT/suites/"
COPY pkg "$TEST_ROOT/pkg/"
COPY integration_test.go "$TEST_ROOT"
COPY benchmark_test.go "$TEST_ROOT"
COPY k8s_test.go "$TEST_ROOT"

RUN --mount=type=cache,target="/root/.cache/go-build" CGO_ENABLED=0 GOOS=linux GOARCH=$TARGETARCH go test -tags bench,k8s -c -o collector-tests

FROM alpine:3.18

ARG TEST_ROOT

RUN apk add docker

COPY --from=builder $TEST_ROOT/collector-tests $TEST_ROOT/collector-tests
COPY images.yml "$TEST_ROOT"

WORKDIR "$TEST_ROOT"

ENTRYPOINT ["./collector-tests"]
