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
	docker build \
		--build-arg REDHAT_USERNAME="${REDHAT_USERNAME}" \
		--build-arg REDHAT_PASSWORD="${REDHAT_PASSWORD}" \
		 $(BUILD_CONTAINER_CACHE_IMAGES:%=--cache-from=%-$@) -t $(BUILD_CONTAINER_TAG)-$* ./build -f $<

build-ubi-container: build/Dockerfile.ubi
	docker build \
		--build-arg REDHAT_USERNAME="${REDHAT_USERNAME}" \
		--build-arg REDHAT_PASSWORD="${REDHAT_PASSWORD}" \
		-t $(BUILD_CONTAINER_TAG)-ubi ./build -f $<

.PHONY: all-build-containers
all-build-containers: build-container $(CUSTOM_FLAVORS:%=build-container-%)

.PHONY: pull-build-container
pull-build-container:
	docker pull $(BUILD_CONTAINER_CACHE_IMAGES) || true

pull-build-container-%: build/Dockerfile.%
	$(BUILD_CONTAINER_CACHE_IMAGES:%=docker pull %-$*) || true

.PHONY: pull-build-containers
pull-build-containers: pull-build-container $(CUSTOM_FLAVORS:%=pull-build-container-%)

.PHONY: push-build-container
push-build-container:
	docker push $(BUILD_CONTAINER_CACHE_IMAGES) || true

push-build-container-%: build/Dockerfile.%
	$(BUILD_CONTAINER_CACHE_IMAGES:%=docker push %-$*) || true

.PHONY: push-build-containers
push-build-containers: push-build-container $(CUSTOM_FLAVORS:%=push-build-container-%)

.PHONY: print-custom-flavors
print-custom-flavors:
	@printf "%s\n" $(CUSTOM_FLAVORS)
