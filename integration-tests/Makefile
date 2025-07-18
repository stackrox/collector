BASE_PATH = ./..
include ../Makefile-constants.mk

ifeq ($(COLLECTOR_REPO),)
COLLECTOR_REPO=quay.io/stackrox-io/collector
endif

ifeq ($(COLLECTOR_IMAGE),)
COLLECTOR_IMAGE=$(COLLECTOR_REPO):$(COLLECTOR_TAG)
endif

ifeq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_TAG=$(shell cat container/QA_TAG | tr -d '\n')
endif

ifeq ($(COLLECTOR_TESTS_REPO),)
COLLECTOR_TESTS_REPO=quay.io/rhacs-eng/collector-tests
endif

ifeq ($(COLLECTOR_TESTS_IMAGE),)
COLLECTOR_TESTS_IMAGE=$(COLLECTOR_TESTS_REPO):$(COLLECTOR_TAG)
endif

GOARCH ?= $(shell go env GOARCH)

SHELL=/bin/bash

# Environment variable COLLECTOR_IMAGE is used by integration-tests
export COLLECTOR_IMAGE
export COLLECTOR_QA_TAG

ALL_TESTS = $(shell go test -tags bench -list .)

#
# Macros to define a test-specific target, based on the list
# of tests provided by the go test utility
#
define make-test-target
$1: docker-clean
	go version
	set -o pipefail; \
		go test -tags bench -timeout 60m -count=1 -v \
		-run $1 2>&1 | tee -a integration-test.log
endef

define make-test-target-dockerized
$1-dockerized: docker-clean build-image
	docker run -it --rm --network=host \
		-v $(realpath $(BASE_PATH))integration-tests/container-logs:/tests/container-logs \
		-v ~/.docker:/root/.docker -v /tmp/:/tmp \
		-v /var/run/docker.sock:/var/run/docker.sock \
		-e COLLECTOR_IMAGE -e COLLECTOR_QA_TAG -e COLLECTOR_LOG_LEVEL=debug \
		$(COLLECTOR_TESTS_IMAGE) \
		-test.run $1 -test.count=1 -test.v -test.timeout 60m 2>&1 | tee -a integration-test.log
endef

# Generates all the specific test targets
$(foreach element,$(ALL_TESTS),$(eval $(call make-test-target,$(element))))

$(foreach element,$(ALL_TESTS),$(eval $(call make-test-target-dockerized,$(element))))

.PHONY: build
build:
	mkdir -p bin/$(GOARCH)
	CGO_ENABLED=0 GOOS=linux GOARCH=$(GOARCH) go test -tags bench,k8s -c -o bin/$(GOARCH)/collector-tests

ARCHS := amd64 arm64 ppc64le s390x

build-%:
	GOARCH=$(*) make build


build-all: $(addprefix build-,$(ARCHS))

.PHONY: build-all build-%

.PHONY: build-image
build-image: build-all
	docker build --platform $(PLATFORM) -t $(COLLECTOR_TESTS_IMAGE) \
		--build-arg QA_TAG=$(shell cat container/QA_TAG) \
		$(CURDIR)

# Run everything on CI, rather than specific tests.
.PHONY: ci-integration-tests
ci-integration-tests: docker-clean
	go version
	set -o pipefail; \
		go test -timeout 120m -v 2>&1 | tee -a integration-test.log

.PHONY: ci-integration-tests-dockerized
ci-integration-tests-dockerized: docker-clean build-image
	docker run -it --rm --network=host \
		-v $(realpath $(BASE_PATH))integration-tests/container-logs:/tests/container-logs \
		-v ~/.docker:/root/.docker \
		-v /tmp/:/tmp \
		-v /var/run/docker.sock:/var/run/docker.sock \
		-e COLLECTOR_IMAGE \
		-e COLLECTOR_QA_TAG \
		-e COLLECTOR_LOG_LEVEL=debug \
		$(COLLECTOR_TESTS_IMAGE) -test.short -test.v | tee -a integration-test.log

.PHONY: ci-benchmarks
ci-benchmarks: baseline
ci-benchmarks: benchmark

.PHONY: docker-clean
docker-clean:
	docker rm -fv container-stats benchmark collector grpc-server 2>/dev/null || true

.PHONY: clean
clean: docker-clean
	rm -rf container-logs/

.PHONY:
benchmark: TestBenchmarkCollector

.PHONY: baseline
baseline: TestBenchmarkBaseline

LOG_FILE ?= integration-test.log
JUNIT_FILE ?= integration-test-report.xml
.PHONY: report
report:
	go install github.com/jstemmer/go-junit-report/v2@latest
	@echo "+ $@"
	@cat $(LOG_FILE) | go-junit-report > `dirname $(LOG_FILE)`/$(JUNIT_FILE)
	@echo
	@echo "Test coverage summary:"
	@grep "^coverage: " -A1 $(LOG_FILE) | grep -v -e '--' | paste -d " "  - -
	@echo
	@echo "Test pass/fail summary:"
	@grep failures `dirname $(LOG_FILE)`/$(JUNIT_FILE) | awk -vOFS="  " 'NF > 0 { $$1 = $$1 } 1'
	@echo
	@echo "`grep 'FAIL  github.com/stackrox/collector' $(LOG_FILE) | wc -l` package(s) detected with compilation or test failures."
	@-grep 'FAIL    github.com/stackrox/collector' $(LOG_FILE) || true
	@echo
	@testerror="$$(grep -e 'Unable to deploy' -e 'FAIL:' $(LOG_FILE) | wc -l)" && test $$testerror -eq 0
