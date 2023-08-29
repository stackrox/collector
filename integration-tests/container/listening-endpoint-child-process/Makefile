BASE_PAT = .
include ../Makefile-constants.mk

.DEFAULT_GOAL = all

COLLECTOR_QA_LISTENING_ENDPOINTS_CHILD_PROCESS_TAG := collector-listening-endpoint-child-process

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_LISTENING_ENDPOINTS_CHILD_PROCESS_TAG=collector-listening-endpoint-child-process-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all: build

.PHONY: build
build:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_LISTENING_ENDPOINTS_CHILD_PROCESS_TAG)-$(IMAGE_ARCH) .

.PHONY: build-and-push
build-and-push: build
	@docker push quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_LISTENING_ENDPOINTS_CHILD_PROCESS_TAG)-$(IMAGE_ARCH)

.PHONY: manifest
manifest:
	@docker manifest create quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_LISTENING_ENDPOINTS_CHILD_PROCESS_TAG) \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_LISTENING_ENDPOINTS_CHILD_PROCESS_TAG)-amd64 \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_LISTENING_ENDPOINTS_CHILD_PROCESS_TAG)-s390x \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_LISTENING_ENDPOINTS_CHILD_PROCESS_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_LISTENING_ENDPOINTS_CHILD_PROCESS_TAG)
