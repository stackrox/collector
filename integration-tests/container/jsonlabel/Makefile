.DEFAULT_GOAL = all

COLLECTOR_QA_JSON_LABEL_TAG := json-label

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_JSON_LABEL_TAG=json-label-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
	@docker buildx build0 --platform=linux/ppc64le,linux/amd64 -f Dockerfile -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG) .

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG)
