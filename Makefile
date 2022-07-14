BASE_PATH = .
include Makefile-constants.mk

MOD_VER_FILE=$(CURDIR)/kernel-modules/kobuild-tmp/MODULE_VERSION.txt

LOCAL_SSH_PORT ?= 2222
DEV_SSH_SERVER_KEY ?= $(CURDIR)/.collector_dev_ssh_host_ed25519_key

export COLLECTOR_VERSION := $(COLLECTOR_TAG)
export MODULE_VERSION := $(shell cat $(CURDIR)/kernel-modules/MODULE_VERSION)

dev-build: image integration-tests-process-network

.PHONY: tag
tag:
	@echo "$(COLLECTOR_TAG)"

.PHONY: container-dockerfile
container-dockerfile:
	envsubst '$${COLLECTOR_VERSION} $${MODULE_VERSION}' \
		< $(CURDIR)/collector/container/Dockerfile.template > \
		$(CURDIR)/collector/container/Dockerfile.gen

.PHONY: builder
builder:
ifdef BUILD_BUILDER_IMAGE
	docker build \
		--cache-from quay.io/stackrox-io/collector-builder:cache \
		--cache-from quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		-t quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		-f "$(CURDIR)/builder/Dockerfile" \
		.
else
	docker pull quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG)
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
	cp kernel-modules/MODULE_VERSION $(MOD_VER_FILE)

.PHONY: $(CURDIR)/$(COLLECTOR_BUILD_CONTEXT)/bundle.tar.gz
$(CURDIR)/$(COLLECTOR_BUILD_CONTEXT)/bundle.tar.gz:
	$(CURDIR)/collector/container/create-bundle.sh $(CURDIR)/collector/container - $(CURDIR)/$(COLLECTOR_BUILD_CONTEXT)/

.PHONY: build-drivers
build-drivers:
	make -C kernel-modules drivers

image: collector unittest container-dockerfile $(MOD_VER_FILE) $(CURDIR)/$(COLLECTOR_BUILD_CONTEXT)/bundle.tar.gz
	make -C collector txt-files
	docker build --build-arg collector_version="$(COLLECTOR_TAG)" \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		-f collector/container/Dockerfile.gen \
		-t quay.io/stackrox-io/collector:$(COLLECTOR_TAG) \
		$(COLLECTOR_BUILD_CONTEXT)

image-dev: COLLECTOR_BUILD_CONTEXT=collector/container/devel
image-dev: collector unittest container-dockerfile $(MOD_VER_FILE) $(CURDIR)/$(COLLECTOR_BUILD_CONTEXT)/bundle.tar.gz
	make -C collector txt-files
	docker build --build-arg collector_version="$(COLLECTOR_TAG)" \
		--build-arg module_version="$(shell cat $(MOD_VER_FILE))" \
		--build-arg BASE_REGISTRY=quay.io \
		--build-arg BASE_IMAGE=centos/centos \
		--build-arg BASE_TAG=stream8 \
		-f collector/container/Dockerfile.gen \
		-t quay.io/stackrox-io/collector:$(COLLECTOR_TAG) \
		$(COLLECTOR_BUILD_CONTEXT)

image-dev-full: image-dev build-drivers
	docker tag quay.io/stackrox-io/collector:$(COLLECTOR_TAG) quay.io/stackrox-io/collector:$(COLLECTOR_TAG)-slim
	docker build \
		--target=probe-layer-1 \
		--tag quay.io/stackrox-io/collector:$(COLLECTOR_TAG)-full \
		--build-arg collector_repo=quay.io/stackrox-io/collector \
		--build-arg collector_version=$(COLLECTOR_TAG) \
		--build-arg module_version=$(shell cat $(CURDIR)/kernel-modules/MODULE_VERSION) \
		--build-arg max_layer_size=300 \
		--build-arg max_layer_depth=1 \
		$(CURDIR)/kernel-modules/container

.PHONY: integration-tests
integration-tests:
	make -C integration-tests tests

.PHONY: integration-tests-benchmark
integration-tests-benchmark:
	make -C integration-tests benchmark

.PHONY: integration-tests-baseline
integration-tests-baseline:
	make -C integration-tests baseline

.PHONY: integration-tests-process-network
integration-tests-process-network:
	make -C integration-tests process-network

.PHONY: integration-tests-missing-proc-scrape
integration-tests-missing-proc-scrape:
	make -C integration-tests missing-proc-scrape

.PHONY: integration-tests-repeat-network
integration-tests-repeat-network:
	make -C integration-tests repeat-network

.PHONY: integration-tests-image-label-json
integration-tests-image-label-json:
	make -C integration-tests image-label-json

.PHONY: integration-tests-report
integration-tests-report:
	make -C integration-tests report

.PHONY: ci-integration-tests
ci-integration-tests: integration-tests-repeat-network \
					  integration-tests-process-network \
					  integration-tests-missing-proc-scrape \
					  integration-tests-image-label-json \
					  integration-tests-report

.PHONY: ci-benchmarks
ci-benchmarks: integration-tests-baseline \
			   integration-tests-benchmark

.PHONY: ci-all-tests
ci-all-tests: ci-benchmarks ci-integration-tests

$(DEV_SSH_SERVER_KEY):
ifeq (,$(wildcard $(DEV_SSH_SERVER_KEY)))
	ssh-keygen -t ed25519 -N '' -f $(DEV_SSH_SERVER_KEY) < /dev/null
endif

.PHONY: start-dev
start-dev: builder teardown-dev $(DEV_SSH_SERVER_KEY)
	docker run -d \
		--name collector_remote_dev \
		--cap-add sys_ptrace -p127.0.0.1:$(LOCAL_SSH_PORT):22 \
		-v $(DEV_SSH_SERVER_KEY):/etc/sshkeys/ssh_host_ed25519_key:ro \
		quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG)

.PHONY: teardown-dev
teardown-dev:
	-docker rm -fv collector_remote_dev

.PHONY: clean
clean: teardown-dev
	make -C collector clean

.PHONY: shfmt-check
shfmt-check:
	shfmt -d $(CURDIR)

.PHONY: shfmt-format
shfmt-format:
	shfmt -w $(CURDIR)

.PHONY: shellcheck-all
shellcheck-all:
	./utilities/shellcheck-all/shellcheck-all.sh

.PHONY: shellcheck-all-dockerized
shellcheck-all-dockerized:
	docker build -t shellcheck-all $(CURDIR)/utilities/shellcheck-all
	docker run --rm -v "$(CURDIR):/scripts" shellcheck-all:latest


# This defines a macro that can be used to add pre-commit targets
# to check staged files (check-<linter name>) or all files (check-<linter name>-all)
define linter
check-$(1):
	pre-commit run $(1)
check-$(1)-all:
	pre-commit run --all-files $(1)
endef

$(eval $(call linter,flake8))
$(eval $(call linter,clang-format))
$(eval $(call linter,shellcheck))
$(eval $(call linter,shfmt))
$(eval $(call linter,circleci_validate))
