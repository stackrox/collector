BUILD_CONTAINER_TAG ?= build-kernel-modules
BUILD_CONTAINER_CACHE_IMAGES ?=

CUSTOM_FLAVORS := $(patsubst build/Dockerfile.%,%,$(wildcard build/Dockerfile.*))

.DEFAULT_GOAL = all
.PHONY: all
all: build-container

.PHONY: build-container
build-container:
	docker build $(BUILD_CONTAINER_CACHE_IMAGES:%=--cache-from=%) -t $(BUILD_CONTAINER_TAG) ./build

build-container-%: build/Dockerfile.%
	docker build $(BUILD_CONTAINER_CACHE_IMAGES:%=--cache-from=%-$%) -t $(BUILD_CONTAINER_TAG)-$* ./build -f $<

.PHONY: all-build-containers
all-build-containers: build-container $(CUSTOM_FLAVORS:%=build-container-%)

.PHONY: print-custom-flavors
print-custom-flavors:
	@printf "%s\n" $(CUSTOM_FLAVORS)
