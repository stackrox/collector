BASE_PATH = .
include Makefile-constants.mk

NPROCS ?= $(shell nproc)

DEV_SSH_SERVER_KEY ?= $(CURDIR)/.collector_dev_ssh_host_ed25519_key
BUILD_BUILDER_IMAGE ?= false

CCACHE_DIR?=$(CURDIR)/.ccache

export COLLECTOR_VERSION := $(COLLECTOR_TAG)

.PHONY: tag
tag:
	@echo "$(COLLECTOR_TAG)"

.PHONY: builder-tag
builder-tag:
	@echo "$(COLLECTOR_BUILDER_TAG)"

.PHONY: container-dockerfile-dev
container-dockerfile-dev:
	sed '1s/ubi-minimal/ubi/' $(CURDIR)/collector/container/Dockerfile > \
		$(CURDIR)/collector/container/Dockerfile.dev

.PHONY: builder
builder:
ifneq ($(BUILD_BUILDER_IMAGE), false)
	docker buildx build --load --platform ${PLATFORM} \
		--build-arg USE_CCACHE="$(USE_CCACHE)" \
		-t quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		-f "$(CURDIR)/builder/Dockerfile" \
		"$(CURDIR)/builder"
endif

collector: check-builder
	make -C collector collector

.PHONY: connscrape
connscrape:
	make -C collector connscrape

.PHONY: unittest
unittest:
	make -C collector unittest

image: collector unittest
	make -C collector txt-files
	docker buildx build --load --platform ${PLATFORM} \
		--build-arg COLLECTOR_VERSION="$(COLLECTOR_TAG)" \
		-f collector/container/Dockerfile \
		-t quay.io/stackrox-io/collector:$(COLLECTOR_TAG) \
		$(COLLECTOR_BUILD_CONTEXT)

image-dev: collector unittest container-dockerfile-dev
	make -C collector txt-files
	docker buildx build --load --platform ${PLATFORM} \
		--build-arg COLLECTOR_VERSION="$(COLLECTOR_TAG)" \
		--build-arg BUILD_TYPE=devel \
		-f collector/container/Dockerfile.dev \
		-t quay.io/stackrox-io/collector:$(COLLECTOR_TAG) \
		$(COLLECTOR_BUILD_CONTEXT)

.PHONY: integration-tests-report
integration-tests-report:
	make -C integration-tests report

.PHONY: ci-integration-tests
ci-integration-tests:
	make -C integration-tests ci-integration-tests

.PHONY: ci-benchmarks
ci-benchmarks:
	make -C integration-tests ci-benchmarks

.PHONY: ci-all-tests
ci-all-tests: ci-benchmarks ci-integration-tests

$(DEV_SSH_SERVER_KEY):
ifeq (,$(wildcard $(DEV_SSH_SERVER_KEY)))
	ssh-keygen -t ed25519 -N '' -f $(DEV_SSH_SERVER_KEY) < /dev/null
endif

.PHONY: start-builder
start-builder: builder teardown-builder
	docker run -d \
		--name $(COLLECTOR_BUILDER_NAME) \
		--pull missing \
		--platform ${PLATFORM} \
		-v $(CCACHE_DIR):/root/.ccache \
		-v $(CURDIR):$(CURDIR) \
		$(if $(LOCAL_SSH_PORT),-p $(LOCAL_SSH_PORT):22 )\
		-w $(CURDIR) \
		--cap-add sys_ptrace \
		quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG)

.PHONY: check-builder
check-builder:
	$(CURDIR)/utilities/check-builder.sh $(COLLECTOR_BUILDER_NAME)

.PHONY: teardown-builder
teardown-builder:
	docker rm -fv $(COLLECTOR_BUILDER_NAME)

.PHONY: clean
clean:
	rm -rf cmake-build*
	make -C collector clean
	make -C integration-tests docker-clean

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

.PHONY: init-githook
init-githook:
	git config core.hooksPath ./githooks/

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
