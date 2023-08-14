.DEFAULT_GOAL = all

COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG := collector-processes-listening-on-ports

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG=collector-processes-listening-on-ports-$(COLLECTOR_QA_TAG)
endif

PLATFORM ?= linux/amd64

.PHONY: all
all:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG) .

.PHONY: push
push:
	@docker buildx build --push --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG) .
