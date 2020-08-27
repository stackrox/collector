BASE_PATH = .
include Makefile-constants.mk

MOD_VER_FILE=$(CURDIR)/kernel-modules/kobuild-tmp/MODULE_VERSION.txt

dev-build: image integration-tests-process-network

dev-build-rhel: image-rhel integration-tests-process-network-rhel

.PHONY: tag
tag:
	@echo "$(COLLECTOR_TAG)"

.PHONY: builder
builder:
ifdef BUILD_BUILDER_IMAGE
	docker build \
		--cache-from stackrox/collector-builder:cache \
		--cache-from stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		-t stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		builder
else
	docker pull stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG)
endif

.PHONY: builder-rhel
builder-rhel:
ifdef BUILD_BUILDER_IMAGE
	docker build \
		--cache-from stackrox/collector-builder:rhel-cache \
		--cache-from stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) \
		-t stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) \
		-f "$(CURDIR)/builder/Dockerfile_rhel" \
		builder
else
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

$(MOD_VER_FILE): build-kernel-modules
	rm -rf kernel-modules/kobuild-tmp/
	mkdir -p kernel-modules/kobuild-tmp/versions-src
	docker run --rm -i \
	  -v "$(CURDIR)/sysdig:/sysdig:ro" \
	  --tmpfs /scratch:exec --tmpfs /output:exec \
	  --env SYSDIG_DIR=/sysdig/src --env SCRATCH_DIR=/scratch --env OUTPUT_DIR=/output \
	  build-kernel-modules prepare-src 2> /dev/null | tail -n 1 > "$(MOD_VER_FILE)"

image: collector unittest $(MOD_VER_FILE)
	make -C collector txt-files
	docker build --build-arg collector_version="$(COLLECTOR_TAG)" \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		-f collector/container/Dockerfile \
		-t stackrox/collector:$(COLLECTOR_TAG) \
		collector/container

.PHONY: $(CURDIR)/collector/container/rhel/bundle.tar.gz
$(CURDIR)/collector/container/rhel/bundle.tar.gz:
	$(CURDIR)/collector/container/rhel/create-bundle.sh $(CURDIR)/collector/container - $@

.PHONY: $(CURDIR)/collector/container/rhel/prebuild.sh
$(CURDIR)/collector/container/rhel/prebuild.sh:
	$(CURDIR)/collector/container/rhel/create-prebuild.sh $@

image-rhel: collector-rhel unittest-rhel $(MOD_VER_FILE) $(CURDIR)/collector/container/rhel/bundle.tar.gz
	make -C collector txt-files
	docker build --build-arg collector_version="rhel-$(COLLECTOR_TAG)" \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		-f collector/container/rhel/Dockerfile \
		-t stackrox/collector-rhel:$(COLLECTOR_TAG) \
		collector/container/rhel

.PHONY: integration-tests
integration-tests:
	make -C integration-tests tests

.PHONY: integration-tests-baseline
integration-tests-baseline:
	make -C integration-tests baseline

.PHONY: integration-tests-process-network
integration-tests-process-network:
	make -C integration-tests process-network

.PHONY: integration-tests-missing-proc-scrape
integration-tests-missing-proc-scrape:
	make -C integration-tests missing-proc-scrape

.PHONY: integration-tests-process-network-rhel
integration-tests-process-network-rhel:
	COLLECTOR_REPO="stackrox/collector-rhel" \
	make -C integration-tests process-network

.PHONY: integration-tests-rhel
integration-tests-rhel:
	COLLECTOR_REPO="stackrox/collector-rhel" \
	make -C integration-tests tests

.PHONY: integration-tests-baseline-rhel
integration-tests-baseline-rhel:
	COLLECTOR_REPO="stackrox/collector-rhel" \
	make -C integration-tests baseline

.PHONY: integration-tests-missing-proc-scrape-rhel
integration-tests-missing-proc-scrape-rhel:
	COLLECTOR_REPO="stackrox/collector-rhel" \
	make -C integration-tests missing-proc-scrape

.PHONY: integration-tests-report
integration-tests-report:
	make -C integration-tests report

.PHONY: clean
clean:
	make -C collector clean
