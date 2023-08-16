BASE_PATH = .
include ../Makefile-constants.mk

.DEFAULT_GOAL = all

COLLECTOR_QA_INIT_TAG := init
COLLECTOR_QA_PERF_TAG := perf
COLLECTOR_QA_BCC_TAG := bcc
COLLECTOR_QA_BPFTRACE_TAG := bpftrace

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_INIT_TAG=init-$(COLLECTOR_QA_TAG)$(IMAGE_SUFFIX)
COLLECTOR_QA_PERF_TAG=perf-$(COLLECTOR_QA_TAG)$(IMAGE_SUFFIX)
COLLECTOR_QA_BCC_TAG=bcc-$(COLLECTOR_QA_TAG)$(IMAGE_SUFFIX)
COLLECTOR_QA_BPFTRACE_TAG=bpftrace-$(COLLECTOR_QA_TAG)$(IMAGE_SUFFIX)
endif

.PHONY: all
all:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_INIT_TAG) --target init -f Dockerfile .
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_PERF_TAG) --target perf -f Dockerfile .
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BCC_TAG) --target bcc -f Dockerfile .
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG) -f Dockerfile.bpftrace .

.PHONY: push
push:
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_INIT_TAG) --target init -f Dockerfile .
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_PERF_TAG) --target perf -f Dockerfile .
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BCC_TAG) --target bcc -f Dockerfile .
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG) -f Dockerfile.bpftrace .

.PHONY: manifest
manifest: manifest-bcc manifest-bpftrace manifest-perf manifest-init

.PHONY: manifest-bcc
manifest-bcc:
	@docker manifest create quay.io/rhacs-eng/collector:performance:$(COLLECTOR_QA_BCC_TAG) \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BCC_TAG)-amd64 \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BCC_TAG)-s390x \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BCC_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BCC_TAG)

.PHONY: manifest-perf
manifest-perf:
	@docker manifest create quay.io/rhacs-eng/collector:performance:$(COLLECTOR_QA_PERF_TAG) \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_PERF_TAG)-amd64 \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_PERF_TAG)-s390x \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_PERF_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_PERF_TAG)

.PHONY: manifest-init
manifest-init:
	@docker manifest create quay.io/rhacs-eng/collector:performance:$(COLLECTOR_QA_INIT_TAG) \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_INIT_TAG)-amd64 \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_INIT_TAG)-s390x \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_INIT_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_INIT_TAG)

.PHONY: manifest-bpftrace
manifest-bpftrace:
	@docker manifest create quay.io/rhacs-eng/collector:performance:$(COLLECTOR_QA_BPFTRACE_TAG) \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG)-amd64 \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG)-s390x \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG)
