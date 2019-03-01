BUILD_CONTAINER_TAG ?= build-kernel-modules
BUILD_CONTAINER_CACHE_IMAGES ?=

.DEFAULT_GOAL = all
.PHONY: all
all: build-container

.PHONY: build-container
build-container:
	docker build $(BUILD_CONTAINER_CACHE_IMAGES:%=--cache-from=%) -t $(BUILD_CONTAINER_TAG) ./build
