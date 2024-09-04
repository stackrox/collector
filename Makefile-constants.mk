ifeq ($(COLLECTOR_BUILDER_TAG),)
COLLECTOR_BUILDER_TAG=master
endif

ifeq ($(COLLECTOR_TAG),)
COLLECTOR_TAG=$(shell git describe --tags --abbrev=10 --dirty)
endif

DOCKER_CLI := $(shell command -v docker 2>/dev/null)

ifeq ($(DOCKER_CLI),)
$(error "docker is required for building")
endif

HOST_ARCH := $(shell docker system info --format '{{.Architecture}}')
PLATFORM ?= "linux/$(HOST_ARCH)"

USE_VALGRIND ?= false
ADDRESS_SANITIZER ?= false
CMAKE_BUILD_TYPE ?= release
CMAKE_BASE_DIR = cmake-build-$(CMAKE_BUILD_TYPE)-$(HOST_ARCH)
TRACE_SINSP_EVENTS ?= false
DISABLE_PROFILING ?= false
BPF_DEBUG_MODE ?= false

COLLECTOR_BUILD_CONTEXT = collector/
COLLECTOR_BUILDER_NAME ?= collector_builder_$(HOST_ARCH)
