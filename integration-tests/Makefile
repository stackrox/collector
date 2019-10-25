SHELL=/bin/bash

ifeq ($(COLLECTOR_TAG),)
export COLLECTOR_TAG=$(shell git describe --tags --abbrev=10 --dirty)
endif

.PHONY: docker-clean
docker-clean:
	docker rm -fv container-stats benchmark collector grpc-server || true

.PHONY: tests
tests: docker-clean
	go version
	set -o pipefail ; \
	go test -timeout 30m -count=1 -v \
	  -run TestCollectorGRPC 2>&1 | tee -a integration-test.log

.PHONY: process-network 
process-network: docker-clean
	go version
	set -o pipefail ; \
	go test -timeout 30m -count=1 -v \
 	  -run TestProcessNetwork 2>&1 | tee -a integration-test.log

.PHONY: baseline
baseline: docker-clean
	go version
	set -o pipefail ; \
	go test -timeout 30m -count=1 -v \
	  -run TestBenchmarkBaseline 2>&1 | tee -a integration-test.log

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
	@grep failures integration-test-report.xml | column -t
	@echo
	@echo "`grep 'FAIL  github.com/stackrox/collector' integration-test.log | wc -l` package(s) detected with compilation or test failures."
	@-grep 'FAIL    github.com/stackrox/collector' integration-test.log || true
	@echo
	@testerror="$$(grep -e 'Unable to deploy' -e 'FAIL:' integration-test.log | wc -l)" && test $$testerror -eq 0

