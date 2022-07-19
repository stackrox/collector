.DEFAULT_GOAL = all

COLLECTOR_QA_INIT_TAG := init
COLLECTOR_QA_PERF_TAG := perf
COLLECTOR_QA_BCC_TAG := bcc
COLLECTOR_QA_BPFTRACE_TAG := bpftrace
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
COLLECTOR_QA_INIT_TAG=init-$(COLLECTOR_QA_TAG)
COLLECTOR_QA_PERF_TAG=perf-$(COLLECTOR_QA_TAG)
COLLECTOR_QA_BCC_TAG=bcc-$(COLLECTOR_QA_TAG)
COLLECTOR_QA_BPFTRACE_TAG=bpftrace-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all:
	@docker build -t quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_INIT_TAG) --target init -f Dockerfile$(ARCHEXTN) .
	@docker build -t quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_PERF_TAG) --target perf -f Dockerfile$(ARCHEXTN) .
	@docker build -t quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_BCC_TAG) --target bcc -f Dockerfile$(ARCHEXTN) .
	@docker build -t quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG) -f Dockerfile.bpftrace$(ARCHEXTN) .

.PHONY: push
push:
	@docker push quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_INIT_TAG)
	@docker push quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_PERF_TAG)
	@docker push quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_BCC_TAG)
	@docker push quay.io/rhacs-eng$(ARCHDIR)/collector-performance:$(COLLECTOR_QA_BPFTRACE_TAG)
