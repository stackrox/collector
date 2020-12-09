BASE_PATH = .
include Makefile-constants.mk

MOD_VER_FILE=$(CURDIR)/kernel-modules/kobuild-tmp/MODULE_VERSION.txt

LOCAL_SSH_PORT ?= 2222
DEV_SSH_SERVER_KEY ?= $(CURDIR)/.collector_dev_ssh_host_ed25519_key

.PHONY: tag
tag:
	@echo "$(COLLECTOR_TAG)"

# create or pull the builder image used to build collector
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

# create or pull the rhel builder image used to build collector
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

# build collector binaries using Docker
collector: builder
	$(MAKE) -C collector container/bin/collector

# build collector rhel binaries using Docker
collector-rhel: builder-rhel
	$(MAKE) -C collector container/bin/collector-rhel

# copy collector binaries built using CLion
collector-dev:
	mkdir -p collector/container/bin collector/container/libs
	docker cp collector_remote_dev:/tmp/cmake-build/collector collector/container/bin/
	docker cp collector_remote_dev:/tmp/cmake-build/EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so collector/container/libs/libsinsp-wrapper.so

# copy rhel collector binaries built using CLion
collector-rhel-dev:
	mkdir -p collector/container/bin collector/container/libs
	docker cp collector_remote_dev:/tmp/cmake-build-rhel/collector collector/container/bin/collector.rhel
	docker cp collector_remote_dev:/tmp/cmake-build-rhel/EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so collector/container/libs/libsinsp-wrapper.so.rhel

txt-files:
	$(MAKE) -C collector txt-files

.PHONY: unittest
unittest:
	$(MAKE) -C collector unittest

.PHONY: unittest-rhel
unittest-rhel:
	$(MAKE) -C collector unittest-rhel

.PHONY: build-kernel-modules
build-kernel-modules:
	$(MAKE) -C kernel-modules build-container

$(MOD_VER_FILE): build-kernel-modules
	./scripts/prepare-module-srcs

collector-image: $(MOD_VER_FILE)
	docker build --build-arg collector_version="$(COLLECTOR_TAG)" \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		-f collector/container/Dockerfile \
		-t stackrox/collector:$(COLLECTOR_TAG) \
		-t stackrox/collector:$(COLLECTOR_TAG)-base \
		collector/container

collector-image-rhel: $(MOD_VER_FILE)
	$(CURDIR)/collector/container/rhel/create-bundle.sh \
		"$(CURDIR)/collector/container" - "$(CURDIR)/collector/container/rhel"
	docker build --build-arg collector_version="rhel-$(COLLECTOR_TAG)" \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		-f collector/container/rhel/Dockerfile \
		-t stackrox/collector-rhel:$(COLLECTOR_TAG) \
		collector/container/rhel

image: collector unittest txt-files collector-image
	docker build \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		--build-arg collector_version="$(COLLECTOR_TAG)" \
		-t "stackrox/collector:$(COLLECTOR_TAG)" \
		"$(CURDIR)/kernel-modules/container"

image-rhel: collector-rhel unittest-rhel txt-files collector-image-rhel

.PHONY: integration-tests
integration-tests:
	$(MAKE) -C integration-tests tests

.PHONY: integration-tests-baseline
integration-tests-baseline:
	$(MAKE) -C integration-tests baseline

.PHONY: integration-tests-process-network
integration-tests-process-network:
	$(MAKE) -C integration-tests process-network

.PHONY: integration-tests-missing-proc-scrape
integration-tests-missing-proc-scrape:
	$(MAKE) -C integration-tests missing-proc-scrape

.PHONY: integration-tests-image-label-json
integration-tests-image-label-json:
	make -C integration-tests image-label-json

.PHONY: integration-tests-process-network-rhel
integration-tests-process-network-rhel:
	COLLECTOR_REPO="stackrox/collector-rhel" \
	$(MAKE) -C integration-tests process-network

.PHONY: integration-tests-rhel
integration-tests-rhel:
	COLLECTOR_REPO="stackrox/collector-rhel" \
	$(MAKE) -C integration-tests tests

.PHONY: integration-tests-baseline-rhel
integration-tests-baseline-rhel:
	COLLECTOR_REPO="stackrox/collector-rhel" \
	$(MAKE) -C integration-tests baseline

.PHONY: integration-tests-missing-proc-scrape-rhel
integration-tests-missing-proc-scrape-rhel:
	COLLECTOR_REPO="stackrox/collector-rhel" \
	$(MAKE) -C integration-tests missing-proc-scrape

.PHONY: integration-tests-report
integration-tests-report:
	$(MAKE) -C integration-tests report

$(DEV_SSH_SERVER_KEY):
ifeq (,$(wildcard $(DEV_SSH_SERVER_KEY)))
	ssh-keygen -t ed25519 -N '' -f $(DEV_SSH_SERVER_KEY) < /dev/null
endif

# Start container for remote docker development in CLion
.PHONY: start-dev
start-dev: builder teardown-dev $(DEV_SSH_SERVER_KEY)
	$(MAKE) -C collector generated-srcs
	mkdir -p collector/cmake-build collector/cmake-build-rhel
	docker run -d \
		--name collector_remote_dev \
		--cap-add sys_ptrace -p127.0.0.1:$(LOCAL_SSH_PORT):22 \
		-v $(DEV_SSH_SERVER_KEY):/etc/sshkeys/ssh_host_ed25519_key:ro \
		stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG)

# Start container, using rhel builder image, for remote docker development in CLion
.PHONY: start-dev-rhel $(DEV_SSH_SERVER_KEY)
start-dev-rhel: builder-rhel teardown-dev
	$(MAKE) -C collector generated-srcs
	docker run -d \
		--name collector_remote_dev \
		--cap-add sys_ptrace -p127.0.0.1:$(LOCAL_SSH_PORT):22 \
		-v $(DEV_SSH_SERVER_KEY):/etc/sshkeys/ssh_host_ed25519_key:ro \
		stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG)

# build collector image with binaries from remote docker dev and locally built probes
image-dev: collector-dev txt-files collector-image probe-archive-dev
	docker build \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		--build-arg collector_version="$(COLLECTOR_TAG)" \
		-t "stackrox/collector:$(COLLECTOR_TAG)" \
		"$(CURDIR)/kernel-modules/container"

# build rhel collector image with binaries from remote docker dev and locally built probes
image-rhel-dev: collector-rhel-dev txt-files probe-archive-dev
	$(CURDIR)/collector/container/rhel/create-bundle.sh \
		"$(CURDIR)/collector/container" \
		"$(CURDIR)/kernel-modules/container/$(shell cat $(MOD_VER_FILE)).tar.gz" \
		"$(CURDIR)/collector/container/rhel"
	docker build \
		--build-arg collector_version="$(COLLECTOR_TAG)" \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		-f collector/container/rhel/Dockerfile \
		-t stackrox/collector-rhel:$(COLLECTOR_TAG) \
		collector/container/rhel

.PHONY: teardown-dev
teardown-dev:
	-docker rm -fv collector_remote_dev

# Copy kernel bundles from ../kernel-packer and build all builder containers
.PHONY: probe-dev
probe-dev: $(MOD_VER_FILE)
	./scripts/copy-kernel-packer-repo-bundles
	./legacy-modules/download.sh
	$(MAKE) -C "${CURDIR}/kernel-modules" all-build-containers

# create an archive of all probes built locally
probe-archive-dev: $(MOD_VER_FILE)
	mkdir -p "$(CURDIR)/probes/$(shell cat $(MOD_VER_FILE))"
	tar czf "$(CURDIR)/kernel-modules/container/$(shell cat $(MOD_VER_FILE)).tar.gz" \
		-C "$(CURDIR)/probes/$(shell cat $(MOD_VER_FILE))" .

.PHONY: clean
clean: teardown-dev
	$(MAKE) -C collector clean
	rm -rf sources patches probes source-archives
