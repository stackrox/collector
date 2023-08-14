.DEFAULT_GOAL = all

COLLECTOR_QA_SCHEDULE_CURLS_TAG := collector-schedule-curls

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_SCHEDULE_CURLS_TAG=collector-schedule-curls-$(COLLECTOR_QA_TAG)
endif

PLATFORM ?= linux/amd64

.PHONY: all
all:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG) .

.PHONY: push
push:
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG) .
