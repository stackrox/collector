
ifeq ($(COLLECTOR_BUILDER_TAG),)
COLLECTOR_BUILDER_TAG=cache
endif

ifeq ($(COLLECTOR_TAG),)
ifeq ($(CIRCLE_TAG),)
COLLECTOR_TAG=$(shell git describe --tags --abbrev=10 --dirty)
else
COLLECTOR_TAG := $(CIRCLE_TAG)
endif
endif
