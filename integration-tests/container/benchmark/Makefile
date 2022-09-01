DEFAULT_GOAL = all

COLLECTOR_QA_BENCHMARK_TAG := phoronix
ARCH := $(shell arch)

ifeq ($(ARCH),x86_64)
ARCHDIR=
ARCHEXTN=
else ifeq ($(ARCH),amd64)
ARCHDIR=
ARCHEXTN=
else
ARCHDIR=/$(ARCH)
ARCHEXTN=.$(ARCH)
endif

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_BENCHMARK_TAG=phoronix-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
	@docker build -f Dockerfile$(ARCHEXTN) -t quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG) .

.PHONY: push
push:
	@docker push quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_BENCHMARK_TAG)
