DEFAULT_GOAL = all

COLLECTOR_QA_BENCHMARK_TAG := phoronix

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_BENCHMARK_TAG=phoronix-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
ifdef BUILD_UPLOAD_MULTIARCH_IMAGES
	@docker buildx build --push --platform=linux/ppc64le,linux/amd64 -f Dockerfile -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG) .
else
	TARGETARCH=$(arch)
	if [ "$TARGETARCH" = "x86_64" ]; then
    	TARGETARCH=amd64
	fi
	@docker build --build-arg TARGETARCH=${TARGETARCH} -f Dockerfile -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG) .
endif

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG)
