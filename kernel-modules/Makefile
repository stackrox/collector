BUILD_CONTAINER_TAG ?= build-kernel-modules
BUILD_CONTAINER_CACHE_IMAGES ?=
DEV_DRIVER_BUILDER ?= fc36

CUSTOM_FLAVORS := $(patsubst build/Dockerfile.%,%,$(wildcard build/Dockerfile.*))

.DEFAULT_GOAL = all
.PHONY: all
all: build-container

.PHONY: build-container
build-container:
	docker build $(BUILD_CONTAINER_CACHE_IMAGES:%=--cache-from=%) -t $(BUILD_CONTAINER_TAG) ./build

build-container-%: build/Dockerfile.%
	docker build $(BUILD_CONTAINER_CACHE_IMAGES:%=--cache-from=%-$@) -t $(BUILD_CONTAINER_TAG)-$* ./build -f $<

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

.PHONY: drivers
drivers: build-container-$(DEV_DRIVER_BUILDER)
	docker run --rm \
		-v $(CURDIR)/..:/collector \
		-v /usr/include/bpf:/usr/include/bpf:ro \
		-v /lib/modules/:/lib/modules/:ro \
		-v /usr/src/:/usr/src/:ro \
		build-kernel-modules-$(DEV_DRIVER_BUILDER):latest \
		/collector/kernel-modules/dev/build-drivers.sh

.PHONY: clean-drivers
clean-drivers:
	rm -rf ../falcosecurity-libs/build/
	rm -rf container/kernel-modules/
