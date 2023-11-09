
ifeq ($(COLLECTOR_BUILDER_TAG),)
COLLECTOR_BUILDER_TAG=master
endif

ifeq ($(COLLECTOR_TAG),)
ifeq ($(CIRCLE_TAG),)
COLLECTOR_TAG=$(shell git describe --tags --abbrev=10 --dirty)
else
COLLECTOR_TAG := $(CIRCLE_TAG)
endif
endif

USE_VALGRIND ?= false
ADDRESS_SANITIZER ?= false
CMAKE_BUILD_TYPE ?= Release
COLLECTOR_APPEND_CID ?= false
PLATFORM ?= linux/amd64
TRACE_SINSP_EVENTS ?= false
DISABLE_PROFILING ?= true

COLLECTOR_BUILD_CONTEXT = collector/
COLLECTOR_BUILDER_NAME ?= collector_builder

export COLLECTOR_PRE_ARGUMENTS
