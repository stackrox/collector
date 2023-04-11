BASE_PATH = .
include Makefile-constants.mk

MOD_VER_FILE=$(CURDIR)/kernel-modules/kobuild-tmp/MODULE_VERSION.txt

export COLLECTOR_VERSION := $(COLLECTOR_TAG)
export MODULE_VERSION := $(shell cat $(CURDIR)/kernel-modules/MODULE_VERSION)

dev-build: image
	make -C integration-tests TestProcessNetwork

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
ifdef BUILD_BUILDER_IMAGE
	docker build \
		--target builder \
		--cache-from quay.io/stackrox-io/collector-builder:cache \
		--cache-from quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		-t quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		-f "$(CURDIR)/builder/Dockerfile" \
		$(CURDIR)/builder/
else
	docker pull quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG)
endif

.PHONY: dev-image
dev-image: builder
	docker build \
		--target dev \
		--cache-from quay.io/stackrox-io/collector-builder:cache \
		--cache-from quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) \
		-t quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG)-dev \
		-f "$(CURDIR)/builder/Dockerfile" \
		$(CURDIR)/builder/

collector: builder
	make -C collector collector

.PHONY: connscrape
connscrape:
	make -C collector connscrape

.PHONY: unittest
unittest:
	make -C collector unittest

.PHONY: build-kernel-modules
build-kernel-modules:
	make -C kernel-modules build-container

.PHONY: build-drivers
build-drivers:
	make -C kernel-modules drivers

image: collector unittest
	make -C collector txt-files
	docker build \
		--build-arg COLLECTOR_VERSION="$(COLLECTOR_TAG)" \
		--build-arg MODULE_VERSION="$(MODULE_VERSION)" \
		-f collector/container/Dockerfile \
		-t quay.io/stackrox-io/collector:$(COLLECTOR_TAG) \
		$(COLLECTOR_BUILD_CONTEXT)

image-dev: collector unittest container-dockerfile-dev
	make -C collector txt-files
	docker build --build-arg collector_version="$(COLLECTOR_TAG)" \
		--build-arg BUILD_TYPE=devel \
		-f collector/container/Dockerfile.dev \
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

.PHONY: start-dev
start-dev: dev-image teardown-dev
	docker run -id \
		--name collector_remote_dev \
		-v $(CURDIR):$(CURDIR) \
		-w $(CURDIR) \
		quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG)-dev

.PHONY: teardown-dev
teardown-dev:
	-docker rm -fv collector_remote_dev

.PHONY: clean
clean: teardown-dev
	rm -rf cmake-build/
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

.PHONY: linters
linters:
	@$(CURDIR)/utilities/lint.sh
