ifeq ($(COLLECTOR_BUILDER_TAG),)
COLLECTOR_BUILDER_TAG=master
endif

ifeq ($(COLLECTOR_TAG),)
COLLECTOR_TAG=$(shell git describe --tags --abbrev=10 --dirty)
endif

HOST_ARCH := $(shell uname -m | sed -e 's/x86_64/amd64/' -e 's/aarch64/arm64/')
PLATFORM ?= "linux/$(HOST_ARCH)"

USE_VALGRIND ?= false
ADDRESS_SANITIZER ?= false
CMAKE_BUILD_TYPE ?= Release
CMAKE_BASE_DIR = cmake-build-$(shell echo $(CMAKE_BUILD_TYPE) | tr A-Z a-z)-$(HOST_ARCH)
TRACE_SINSP_EVENTS ?= false
DISABLE_PROFILING ?= false
BPF_DEBUG_MODE ?= false

COLLECTOR_BUILD_CONTEXT = collector/
COLLECTOR_BUILDER_NAME ?= collector_builder_$(HOST_ARCH)
