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
	make -C collector container/bin/collector

# build collector rhel binaries using Docker
collector-rhel: builder-rhel
	make -C collector container/bin/collector-rhel

# Copy collector binaries built using remote dev environment
collector-dev:
	mkdir -p collector/container/bin collector/container/libs
	docker cp collector_remote_dev:/tmp/cmake-build/collector collector/container/bin/
	docker cp collector_remote_dev:/tmp/cmake-build/EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so collector/container/libs/libsinsp-wrapper.so
	docker cp collector_remote_dev:/THIRD_PARTY_NOTICES - | tar -x --strip-components 1 -C collector/container/THIRD_PARTY_NOTICES

# Copy rhel collector binaries built using remote dev environment
collector-rhel-dev:
	mkdir -p collector/container/bin collector/container/libs
	docker cp collector_remote_dev:/tmp/cmake-build-rhel/collector collector/container/bin/collector.rhel
	docker cp collector_remote_dev:/tmp/cmake-build-rhel/EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so collector/container/libs/libsinsp-wrapper.so.rhel
	docker cp collector_remote_dev:/THIRD_PARTY_NOTICES - | tar -x --strip-components 1 -C collector/container/THIRD_PARTY_NOTICES.rhel

txt-files:
	make -C collector txt-files

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

.PHONY: integration-tests-image-label-json
integration-tests-image-label-json:
	make -C integration-tests image-label-json

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

$(DEV_SSH_SERVER_KEY):
ifeq (,$(wildcard $(DEV_SSH_SERVER_KEY)))
	ssh-keygen -t ed25519 -N '' -f $(DEV_SSH_SERVER_KEY) < /dev/null
endif

# Start container for remote docker development in CLion
.PHONY: start-dev
start-dev: builder teardown-dev $(DEV_SSH_SERVER_KEY)
	make -C collector generated-srcs
	mkdir -p collector/cmake-build collector/cmake-build-rhel
	docker run -d \
		--name collector_remote_dev \
		--cap-add sys_ptrace -p127.0.0.1:$(LOCAL_SSH_PORT):22 \
		-v $(DEV_SSH_SERVER_KEY):/etc/sshkeys/ssh_host_ed25519_key:ro \
		stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG)

# Start container, using rhel builder image, for remote docker development in CLion
.PHONY: start-dev-rhel $(DEV_SSH_SERVER_KEY)
start-dev-rhel: builder-rhel teardown-dev
	make -C collector generated-srcs
	docker run -d \
		--name collector_remote_dev \
		--cap-add sys_ptrace -p127.0.0.1:$(LOCAL_SSH_PORT):22 \
		-v $(DEV_SSH_SERVER_KEY):/etc/sshkeys/ssh_host_ed25519_key:ro \
		stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG)

# Build collector image with binaries from remote docker dev and locally built probes
image-dev: txt-files collector-dev collector-image probe-archive-dev
	docker build \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		--build-arg collector_version="$(COLLECTOR_TAG)" \
		--build-arg max_layer_depth=7 \
		--build-arg max_layer_size=300 \
		-t "stackrox/collector:$(COLLECTOR_TAG)" \
		"$(CURDIR)/kernel-modules/container"

# Build collector image and binaries, with locally built probes
image-local: collector unittest txt-files collector-image probe-archive-dev
	docker build \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		--build-arg collector_version="$(COLLECTOR_TAG)" \
		--build-arg max_layer_depth=7 \
		--build-arg max_layer_size=300 \
		-t "stackrox/collector:$(COLLECTOR_TAG)" \
		"$(CURDIR)/kernel-modules/container"

# Build rhel collector image with binaries built in remote dev environment and locally built probes
image-rhel-dev: txt-files collector-rhel-dev probe-archive-dev
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
	make -C "${CURDIR}/kernel-modules" all-build-containers

# Create an archive of all probes built locally
.PHONY: probe-archive-dev
probe-archive-dev: $(MOD_VER_FILE)
	mkdir -p "$(CURDIR)/kernel-modules/container/kernel-modules"
	cp "$(CURDIR)/probes/$(shell cat $(MOD_VER_FILE))/"* "$(CURDIR)/kernel-modules/container/kernel-modules/" || true

.PHONY: clean
clean: teardown-dev
	make -C collector clean
	rm -rf sources patches probes source-archives kobuild
	rm -rf "$(CURDIR)/kernel-modules/container/kernel-modules"
