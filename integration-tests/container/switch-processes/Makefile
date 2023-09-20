BASE_PAT = .
include ../Makefile-constants.mk

.DEFAULT_GOAL = all

COLLECTOR_QA_SWITCH_PROCESSES_TAG := collector-switch-processes

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_SWITCH_PROCESSES_TAG=collector-switch-processes-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all: build

.PHONY: build
build:
	@docker buildx build --load --platform $(PLATFORM) \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SWITCH_PROCESSES_TAG) .

.PHONY: build-and-push
build-and-push:
	@docker buildx build --push --platform $(PLATFORM) \
		-t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SWITCH_PROCESSES_TAG) .

