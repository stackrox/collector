BASE_PAT = .
include ../Makefile-constants.mk

.DEFAULT_GOAL = all

COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG := collector-processes-listening-on-ports

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG=collector-processes-listening-on-ports-$(COLLECTOR_QA_TAG)$(IMAGE_SUFFIX)
endif

.PHONY: all
all: build

.PHONY: build
build:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG) .

.PHONY: push
push:
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG) .

.PHONY: manifest
manifest:
	@docker manifest create quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG) \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG)-amd64 \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG)-s390x \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG)
