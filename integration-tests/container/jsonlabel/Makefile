BASE_PATH = .
include ../Makefile-constants.mk

.DEFAULT_GOAL = all

COLLECTOR_QA_JSON_LABEL_TAG := json-label

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_JSON_LABEL_TAG=json-label-$(COLLECTOR_QA_TAG)$(IMAGE_SUFFIX)
endif

.PHONY: all
all: build

.PHONY: build
build:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG) .

.PHONY: push
push:
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG) .

.PHONY: manifest
manifest:
	@docker manifest create quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG) \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG)-amd64 \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG)-s390x \
		quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG)
