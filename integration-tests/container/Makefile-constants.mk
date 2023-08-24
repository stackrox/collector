PLATFORM ?= linux/amd64
IMAGE_ARCH = $(word 2,$(subst /, ,$(PLATFORM)))

# path is relative to the build directory (i.e. subdirectories of integration-test/container)
COLLECTOR_QA_TAG ?= $(shell cat "../QA_TAG")
