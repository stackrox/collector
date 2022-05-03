DEFAULT_GOAL = all

COLLECTOR_QA_BENCHMARK_TAG := phoronix

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_BENCHMARK_TAG=phoronix-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
	@docker build -t quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG) .

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG)
