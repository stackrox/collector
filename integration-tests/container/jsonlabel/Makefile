.DEFAULT_GOAL = all

COLLECTOR_QA_JSON_LABEL_TAG := json-label

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_JSON_LABEL_TAG=json-label-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
ifdef BUILD_UPLOAD_MULTIARCH_IMAGES
	@docker buildx build --push --platform=linux/ppc64le,linux/amd64 -f Dockerfile -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG) .
else
	@docker build -f Dockerfile -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG) .
endif

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_JSON_LABEL_TAG)
