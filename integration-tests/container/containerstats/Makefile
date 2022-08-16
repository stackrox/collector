.DEFAULT_GOAL = all

COLLECTOR_QA_STATS_TAG := stats

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_STATS_TAG=stats-$(COLLECTOR_QA_TAG)
endif

TARGETARCH := $(arch)
ifeq ($(ARCH),x86_64)
TARGETARCH=amd64
endif

.PHONY: all
all:
ifdef BUILD_UPLOAD_MULTIARCH_IMAGES
	@docker buildx build --push --platform=linux/ppc64le,linux/amd64 -f Dockerfile -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_STATS_TAG) .
else
	@docker build --build-arg TARGETARCH=${TARGETARCH} -f Dockerfile -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_STATS_TAG) .
endif

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_STATS_TAG)
