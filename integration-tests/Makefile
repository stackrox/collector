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
COLLECTOR_TESTS_REPO=quay.io/stackrox-io/collector-tests
endif

ifeq ($(COLLECTOR_TESTS_IMAGE),)
COLLECTOR_TESTS_IMAGE=$(COLLECTOR_TESTS_REPO):$(COLLECTOR_TAG)
endif

SHELL=/bin/bash

# Environment variable COLLECTOR_IMAGE is used by integration-tests
export COLLECTOR_IMAGE

ALL_TESTS = $(shell go test -tags bench -list .)

#
# Macro to define a test-specific target, based on the list
# of tests provided by the go test utility
#
define make-test-target
$1: docker-clean
	go version
	set -o pipefail; \
		go test -tags bench -timeout 60m -count=1 -v \
		-run $1 2>&1 | tee -a integration-test.log
endef

# Generates all the specific test targets
$(foreach element,$(ALL_TESTS),$(eval $(call make-test-target,$(element))))

.PHONY: build
build:
	mkdir -p bin
	go test -tags bench -c -o bin/collector-tests

.PHONY: build-image
build-image:
	docker build -t $(COLLECTOR_TESTS_IMAGE) \
		--build-arg QA_TAG=$(shell cat container/QA_TAG) \
		--build-arg COLLECTOR_IMAGE=$(COLLECTOR_IMAGE) \
		.

# Run everything on CI, rather than specific tests.
.PHONY: ci-integration-tests
ci-integration-tests: docker-clean
	go version
	set -o pipefail; \
		go test -timeout 120m -v 2>&1 | tee -a integration-test.log

.PHONY: ci-benchmarks
ci-benchmarks: baseline
ci-benchmarks: benchmark

.PHONY: docker-clean
docker-clean:
	docker rm -fv container-stats benchmark collector grpc-server 2>/dev/null || true

.PHONY:
benchmark: TestBenchmarkCollector

.PHONY: baseline
baseline: TestBenchmarkBaseline

LOG_FILE ?= integration-test.log
.PHONY: report
report:
	go install github.com/jstemmer/go-junit-report/v2@latest
	@echo "+ $@"
	@cat $(LOG_FILE) | go-junit-report > `dirname $(LOG_FILE)`/integration-test-report.xml
	@echo
	@echo "Test coverage summary:"
	@grep "^coverage: " -A1 $(LOG_FILE) | grep -v -e '--' | paste -d " "  - -
	@echo
	@echo "Test pass/fail summary:"
	@grep failures `dirname $(LOG_FILE)`/integration-test-report.xml | awk -vOFS="  " 'NF > 0 { $$1 = $$1 } 1'
	@echo
	@echo "`grep 'FAIL  github.com/stackrox/collector' $(LOG_FILE) | wc -l` package(s) detected with compilation or test failures."
	@-grep 'FAIL    github.com/stackrox/collector' $(LOG_FILE) || true
	@echo
	@testerror="$$(grep -e 'Unable to deploy' -e 'FAIL:' $(LOG_FILE) | wc -l)" && test $$testerror -eq 0
