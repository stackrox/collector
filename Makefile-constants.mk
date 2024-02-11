
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

PLATFORM ?= $(shell docker system info --format {{.OSType}}/{{.Architecture}})

USE_VALGRIND ?= false
ADDRESS_SANITIZER ?= false
CMAKE_BUILD_TYPE ?= Release
CMAKE_BASE_DIR = cmake-build-$(shell echo $(CMAKE_BUILD_TYPE) | tr A-Z a-z)-$(subst /,-,$(PLATFORM))
COLLECTOR_APPEND_CID ?= false
TRACE_SINSP_EVENTS ?= false
DISABLE_PROFILING ?= false

COLLECTOR_BUILD_CONTEXT = collector/
COLLECTOR_BUILDER_NAME ?= collector_builder_$(subst /,_,$(PLATFORM))
COLLECTOR_BUILDER_REPO=quay.io/stackrox-io/collector-builder
COLLECTOR_BUILDER_IMAGE="$(COLLECTOR_BUILDER_REPO):$(COLLECTOR_BUILDER_TAG)"

export COLLECTOR_PRE_ARGUMENTS
