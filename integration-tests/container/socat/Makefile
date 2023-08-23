BASE_PATH = .
include ../Makefile-constants.mk

.DEFAULT_GOAL = all

COLLECTOR_QA_SOCAT_TAG := socat

ifneq ($(COLLECTOR_QA_TAG),)
COLLECTOR_QA_SOCAT_TAG=socat-$(COLLECTOR_QA_TAG)
endif

.PHONY: all
all: build

.PHONY: build
build:
	@docker buildx build --load --platform ${PLATFORM} \
                -t quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SOCAT_TAG)-$(IMAGE_ARCH) .

.PHONY: build-and-push
build-and-push: build
	@docker push quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SOCAT_TAG)-$(IMAGE_ARCH)

.PHONY: manifest
manifest:
	@docker manifest create quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SOCAT_TAG) \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SOCAT_TAG)-amd64 \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SOCAT_TAG)-s390x \
		quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SOCAT_TAG)-ppc64le

	@docker manifest push quay.io/rhacs-eng/qa-multi-arch:$(COLLECTOR_QA_SOCAT_TAG)
