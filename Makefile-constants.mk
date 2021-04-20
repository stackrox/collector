
ifeq ($(COLLECTOR_BUILDER_TAG),)
COLLECTOR_BUILDER_TAG=cache
endif

ifeq ($(COLLECTOR_TAG),)
COLLECTOR_TAG=$(shell git describe --tags --abbrev=10 --dirty)
endif

SYSDIG_VERSION=0.26.4
