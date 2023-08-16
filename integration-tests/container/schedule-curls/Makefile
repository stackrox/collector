BASE_PATH = .
include ../Makefile-constants.mk

.DEFAULT_GOAL = all

COLLECTOR_QA_SCHEDULE_CURLS_TAG := collector-schedule-curls

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_SCHEDULE_CURLS_TAG=collector-schedule-curls-$(COLLECTOR_QA_TAG)$(IMAGE_SUFFIX)
endif

.PHONY: all
all:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG) .

.PHONY: push
push:
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG) .

.PHONY: manifest
manifest:
	@docker manifest create quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG) \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG)-amd64 \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG)-s390x \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG)
