BASE_PATH = .
include Makefile-constants.mk

MOD_VER_FILE=$(CURDIR)/kernel-modules/kobuild-tmp/MODULE_VERSION.txt

LOCAL_SSH_PORT ?= 2222
DEV_SSH_SERVER_KEY ?= $(CURDIR)/.collector_dev_ssh_host_ed25519_key

dev-build: image integration-tests-process-network

.PHONY: tag
tag:
	@echo "$(COLLECTOR_TAG)"

.PHONY: builder
builder:
ifdef BUILD_BUILDER_IMAGE
	docker build \
		--cache-from stackrox/collector-builder:cache \
		--cache-from stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		--build-arg USE_VALGRIND=$(USE_VALGRIND) \
		-t stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		-f "$(CURDIR)/builder/Dockerfile" \
		builder
else
	docker pull stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG)
endif

collector: builder
	make -C collector container/bin/collector

.PHONY: unittest
unittest:
	make -C collector unittest

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

.PHONY: $(CURDIR)/collector/container/rhel/bundle.tar.gz
$(CURDIR)/collector/container/rhel/bundle.tar.gz:
	$(CURDIR)/collector/container/rhel/create-bundle.sh $(CURDIR)/collector/container - $(CURDIR)/collector/container/rhel/

.PHONY: $(CURDIR)/collector/container/rhel/prebuild.sh
$(CURDIR)/collector/container/rhel/prebuild.sh:
	$(CURDIR)/collector/container/rhel/create-prebuild.sh $@

image: collector unittest $(MOD_VER_FILE) $(CURDIR)/collector/container/rhel/bundle.tar.gz
	make -C collector txt-files
	docker build --build-arg collector_version="$(COLLECTOR_TAG)" \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		--build-arg USE_VALGRIND=$(USE_VALGRIND) \
		-f collector/container/rhel/Dockerfile \
		-t stackrox/collector:$(COLLECTOR_TAG) \
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

.PHONY: integration-tests-image-label-json
integration-tests-image-label-json:
	make -C integration-tests image-label-json

.PHONY: integration-tests-report
integration-tests-report:
	make -C integration-tests report

$(DEV_SSH_SERVER_KEY):
ifeq (,$(wildcard $(DEV_SSH_SERVER_KEY)))
	ssh-keygen -t ed25519 -N '' -f $(DEV_SSH_SERVER_KEY) < /dev/null
endif

.PHONY: start-dev
start-dev: builder teardown-dev $(DEV_SSH_SERVER_KEY)
	make -C collector generated-srcs
	docker run -d \
		--name collector_remote_dev \
		--cap-add sys_ptrace -p127.0.0.1:$(LOCAL_SSH_PORT):22 \
		-v $(DEV_SSH_SERVER_KEY):/etc/sshkeys/ssh_host_ed25519_key:ro \
		stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG)

.PHONY: teardown-dev
teardown-dev:
	-docker rm -fv collector_remote_dev

.PHONY: clean
clean: teardown-dev
	make -C collector clean

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

