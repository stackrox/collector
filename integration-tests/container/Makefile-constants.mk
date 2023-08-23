PLATFORM ?= linux/amd64
IMAGE_ARCH = $(word 2,$(subst /, ,$(PLATFORM)))
COLLECTOR_QA_TAG ?= $(shell cat "../QA_TAG")
