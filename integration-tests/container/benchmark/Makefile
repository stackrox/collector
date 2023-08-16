BASE_PATH = .
include ../Makefile-constants.mk

DEFAULT_GOAL = all

COLLECTOR_QA_BENCHMARK_TAG := phoronix

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_BENCHMARK_TAG=phoronix-$(COLLECTOR_QA_TAG)$(IMAGE_SUFFIX)
endif

.PHONY: all
all: build

.PHONY: build
build:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG)$(IMAGE_SUFFIX) .

.PHONY: push
push:
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG)$(IMAGE_SUFFIX) .

.PHONY: manifest
manifest:
	@docker manifest create quay.io/rhacs-eng/collector:performance:$(COLLECTOR_QA_BENCHMARK_TAG) \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG)-amd64 \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG)-s390x \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG)
