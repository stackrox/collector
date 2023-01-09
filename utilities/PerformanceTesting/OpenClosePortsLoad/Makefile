.DEFAULT_GOAL = all

COLLECTOR_QA_OPEN_CLOSE_PORTS_LOAD_TAG := collector-open-close-ports-load

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_OPEN_CLOSE_PORTS_LOAD_TAG=collector-open-close-ports-load-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
	@docker build -t quay.io/rhacs-eng/qa:$(COLLECTOR_QA_OPEN_CLOSE_PORTS_LOAD_TAG) .

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/qa:$(COLLECTOR_QA_OPEN_CLOSE_PORTS_LOAD_TAG)

