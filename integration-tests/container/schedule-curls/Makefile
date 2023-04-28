.DEFAULT_GOAL = all

COLLECTOR_QA_SCHEDULE_CURLS_TAG := collector-schedule-curls

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_SCHEDULE_CURLS_TAG=collector-schedule-curls-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
	@docker build -t quay.io/rhacs-eng/qa:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG) .

.PHONY: push
push:
	@docker push quay.io/rhacs-eng/qa:$(COLLECTOR_QA_SCHEDULE_CURLS_TAG)
