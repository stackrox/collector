.DEFAULT_GOAL = all

COLLECTOR_QA_STATS_TAG := stats

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_STATS_TAG=stats-$(COLLECTOR_QA_TAG)
endif

PLATFORM ?= linux/amd64

.PHONY: all
all:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_STATS_TAG) .

.PHONY: push
push:
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_STATS_TAG) .
