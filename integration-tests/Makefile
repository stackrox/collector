BASE_PATH = ./..
include ../Makefile-constants.mk

ifeq ($(COLLECTOR_REPO),)
COLLECTOR_REPO=stackrox/collector
endif

ifeq ($(COLLECTOR_IMAGE),)
COLLECTOR_IMAGE=$(COLLECTOR_REPO):$(COLLECTOR_TAG)
endif

SHELL=/bin/bash

# Environment variable COLLECTOR_IMAGE is used by integration-tests
export COLLECTOR_IMAGE

.PHONY: docker-clean
docker-clean:
	docker rm -fv container-stats benchmark collector grpc-server || true

.PHONY: tests
tests: docker-clean
	go version
	set -o pipefail ; \
	go test -timeout 30m -count=1 -v \
	  -run TestCollectorGRPC 2>&1 | tee -a integration-test.log || true ;
	ls container-logs/* || true ;
	cat container-logs/TestCollectorGRPC-collector.log || true

.PHONY: process-network
process-network: docker-clean
	go version
	set -o pipefail ; \
	go test -timeout 30m -count=1 -v \
 	  -run TestProcessNetwork 2>&1 | tee -a integration-test.log || true ;
	ls container-logs/* || true

.PHONY: baseline
baseline: docker-clean
	go version
	set -o pipefail ; \
	go test -timeout 30m -count=1 -v \
	  -run TestBenchmarkBaseline 2>&1 | tee -a integration-test.log || true;
	ls container-logs/* || true

.PHONY: image-label-json
image-label-json: docker-clean
	go version
	set -o pipefail ; \
	go test -timeout 30m -count=1 -v \
	  -run TestImageLabelJSON 2>&1 | tee -a integration-test.log || true ;
	ls container-logs/* || true ;
	cat container-logs/TestImageLabelJSON-collector.log || true

.PHONY: missing-proc-scrape
missing-proc-scrape: docker-clean
	./scripts/create-fake-proc-dir.sh
	go version
	set -o pipefail ; \
	go test -timeout 30m -count=1 -v \
	  -run TestMissingProcScrape 2>&1 | tee -a integration-test.log || true ;
	ls container-logs/* || true ;
	cat container-logs/TestMissingProcScrape-collector.log || true

.PHONY: report
report:
	go get -u github.com/jstemmer/go-junit-report
	@echo "+ $@"
	@cat integration-test.log | go-junit-report > integration-test-report.xml
	@echo
	@echo "Test coverage summary:"
	@grep "^coverage: " -A1 integration-test.log | grep -v -e '--' | paste -d " "  - -
	@echo
	@echo "Test pass/fail summary:"
	@grep failures integration-test-report.xml | awk -vOFS="  " 'NF > 0 { $$1 = $$1 } 1'
	@echo
	@echo "`grep 'FAIL  github.com/stackrox/collector' integration-test.log | wc -l` package(s) detected with compilation or test failures."
	@-grep 'FAIL    github.com/stackrox/collector' integration-test.log || true
	@echo
	@testerror="$$(grep -e 'Unable to deploy' -e 'FAIL:' integration-test.log | wc -l)" && test $$testerror -eq 0

