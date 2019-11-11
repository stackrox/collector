ROOT_DIR = .

ifeq ($(COLLECTOR_TAG),)
export COLLECTOR_TAG=$(shell git describe --tags --abbrev=10 --dirty)
endif
# Environment variable COLLECTOR_TAG is used by integration-tests
export COLLECTOR_TAG

ifeq ($(COLLECTOR_BUILDER_TAG),)
export COLLECTOR_BUILDER_TAG=cache
endif

ifdef CI
BUILD_BUILDER_IMAGE=true
endif

dev-build: image integration-tests-process-network

dev-build-rhel: image-rhel integration-tests-process-network-rhel

.PHONY: tag
tag:
	@git describe --tags --abbrev=10 --dirty

.PHONY: builder
builder:
ifdef BUILD_BUILDER_IMAGE
	@echo "Building collector-builder image"
	docker build \
	  --cache-from stackrox/collector-builder:cache \
	  --cache-from stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) \
	  -t stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) \
	  builder
else
	@echo "Using collector-builder image"
	docker pull stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) 
endif

.PHONY: builder-rhel
builder-rhel:
ifdef BUILD_BUILDER_IMAGE
	@echo "Building collector-builder rhel image"
	docker build \
	  --cache-from stackrox/collector-builder:rhel-cache \
	  --cache-from stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) \
	  -t stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) \
	  -f "$(CURDIR)/builder/Dockerfile_rhel" \
	  builder
else
	@echo "Using collector-builder image"
	docker pull stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) 
endif

collector: builder
	make -C collector container/bin/collector

collector-rhel: builder-rhel
	make -C collector container/bin/collector-rhel

.PHONY: unittest
unittest:
	make -C collector unittest

.PHONY: unittest-rhel
unittest-rhel:
	make -C collector unittest-rhel

.PHONY: build-kernel-modules
build-kernel-modules:
	make -C kernel-modules build-container

.PHONY: prepare-src
prepare-src: build-kernel-modules
	rm -rf kernel-modules/kobuild-tmp/
	mkdir -p kernel-modules/kobuild-tmp/versions-src
	docker run --rm -i \
	  -v "$(CURDIR)/sysdig:/sysdig:ro" \
	  -v "$(CURDIR)/kernel-modules/kobuild-tmp/versions-src:/output" \
	  --tmpfs /scratch:exec \
	  --env SYSDIG_DIR=/sysdig/src --env SCRATCH_DIR=/scratch --env OUTPUT_DIR=/output \
	  build-kernel-modules prepare-src 2> /dev/null | tail -n 1 > kernel-modules/kobuild-tmp/MODULE_VERSION.txt

.PHONY: module-version
module-version: prepare-src
	$(eval MODULE_VERSION = $(shell cat ${ROOT_DIR}/kernel-modules/kobuild-tmp/MODULE_VERSION.txt))
	@echo "MODULE VERSION is $(MODULE_VERSION)"

image: collector unittest module-version
	make -C collector txt-files
	docker build --build-arg collector_version="$(COLLECTOR_TAG)" \
	  --build-arg module_version="$(MODULE_VERSION)" \
	  -f collector/container/Dockerfile \
	  -t stackrox/collector:$(COLLECTOR_TAG) \
	  collector/container

image-rhel: collector-rhel unittest-rhel module-version
	make -C collector txt-files
	docker build --build-arg collector_version="rhel-$(COLLECTOR_TAG)" \
	  --build-arg module_version="$(MODULE_VERSION)" \
	  -f collector/container/Dockerfile_rhel \
	  -t stackrox/collector:rhel-$(COLLECTOR_TAG) \
	  collector/container

.PHONY: integration-tests
integration-tests:
	make -C integration-tests tests

.PHONY: integration-tests-baseline
integration-tests-baseline:
	make -C integration-tests baseline

.PHONY: integration-tests-process-network
integration-tests-process-network:
	make -C integration-tests process-network

.PHONY: integration-tests-process-network-rhel
integration-tests-process-network-rhel:
	COLLECTOR_TAG="rhel-$(COLLECTOR_TAG)" \
	make -C integration-tests process-network

.PHONY: integration-tests-rhel
integration-tests-rhel:
	COLLECTOR_TAG="rhel-$(COLLECTOR_TAG)" \
	  make -C integration-tests tests

.PHONY: integration-tests-baseline-rhel
integration-tests-baseline-rhel:
	COLLECTOR_TAG="rhel-$(COLLECTOR_TAG)" \
	  make -C integration-tests baseline

.PHONY: integration-tests-report
integration-tests-report:
	make -C integration-tests report

.PHONY: clean 
clean:
	make -C collector clean 

