.DEFAULT_GOAL = all

COLLECTOR_QA_STATS_TAG := stats

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_STATS_TAG=stats-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
	@docker buildx build --push --platform=linux/ppc64le,linux/amd64 -f Dockerfile -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_STATS_TAG) .

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_STATS_TAG)
