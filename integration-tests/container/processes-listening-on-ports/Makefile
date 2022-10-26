.DEFAULT_GOAL = all

COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS := collector-processes-listening-on-ports

COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG=collector-processes-listening-on-ports
ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG=collector-processes-listening-on-ports-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
	@docker build -t quay.io/rhacs-eng/qa:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG) .

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/qa:$(COLLECTOR_QA_PROCESSES_LISTENING_ON_PORTS_TAG)

