BASE_PATH = .
include ../Makefile-constants.mk

.DEFAULT_GOAL = all

COLLECTOR_QA_INIT_TAG := init
COLLECTOR_QA_PERF_TAG := perf
COLLECTOR_QA_BCC_TAG := bcc
COLLECTOR_QA_BPFTRACE_TAG := bpftrace

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_INIT_TAG=init-$(COLLECTOR_QA_TAG)
COLLECTOR_QA_PERF_TAG=perf-$(COLLECTOR_QA_TAG)
COLLECTOR_QA_BCC_TAG=bcc-$(COLLECTOR_QA_TAG)
COLLECTOR_QA_BPFTRACE_TAG=bpftrace-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all: build

.PHONY: build
build:
	@docker buildx build --load --platform $(PLATFORM) \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_INIT_TAG) --target init -f Dockerfile .
	@docker buildx build --load --platform $(PLATFORM) \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_PERF_TAG) --target perf -f Dockerfile .
	@docker buildx build --load --platform $(PLATFORM) \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BCC_TAG) --target bcc -f Dockerfile .
	@docker buildx build --load --platform $(PLATFORM) \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG) -f Dockerfile.bpftrace .

.PHONY: build-and-push
build-and-push: init perf bcc bpftrace

.PHONY: init
init:
	docker buildx build --push --platform $(PLATFORM) \
		-t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_INIT_TAG) --target init -f Dockerfile .

.PHONY: perf
perf:
	docker buildx build --push --platform $(PLATFORM) \
		-t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_PERF_TAG) --target perf -f Dockerfile .

.PHONY: bcc
bcc:
	docker buildx build --push --platform $(PLATFORM) \
		-t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BCC_TAG) --target bcc -f Dockerfile .

.PHONY: bpftrace
bpftrace:
	docker buildx build --push --platform $(PLATFORM) \
		-t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG) -f Dockerfile.bpftrace .
