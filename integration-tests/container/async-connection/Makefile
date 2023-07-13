TAG := collector-async-connection

ifneq ($(COLLECTOR_QA_TAG),)
TAG=collector-async-connection-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
	@docker build -t quay.io/rhacs-eng/qa:$(TAG) .

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/qa:$(TAG)

