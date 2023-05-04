BASE_PATH = $(realpath ./..)
include ../Makefile-constants.mk

NPROCS ?= $(shell nproc)

CMAKE_BASE_DIR = cmake-build
CMAKE_DIR= $(BASE_PATH)/$(CMAKE_BASE_DIR)
COLLECTOR_BIN_DIR = $(CMAKE_DIR)/collector
LIBSINSP_BIN_DIR = $(CMAKE_DIR)/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp
SRC_MOUNT_DIR = /tmp/collector

HDRS := $(wildcard lib/*.h) $(shell find falcosecurity-libs/userspace -name '*.h')

SRCS := $(wildcard lib/*.cpp) collector.cpp

COLLECTOR_BUILD_DEPS := $(HDRS) $(SRCS) $(shell find falcosecurity-libs -name '*.h' -o -name '*.cpp' -o -name '*.c')

.SUFFIXES:

cmake-build/collector: $(COLLECTOR_BUILD_DEPS)
	docker rm -fv build_collector || true
	docker run --rm --platform ${PLATFORM} --name build_collector \
		-v "$(BASE_PATH):$(SRC_MOUNT_DIR)" \
		-e NPROCS=$(NPROCS) \
		-e SRC_ROOT_DIR="$(SRC_MOUNT_DIR)" \
		-e CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		-e USE_VALGRIND=$(USE_VALGRIND) \
		-e ADDRESS_SANITIZER=$(ADDRESS_SANITIZER) \
		-e COLLECTOR_APPEND_CID=$(COLLECTOR_APPEND_CID) \
		-e DISABLE_PROFILING="true" \
		quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) "$(SRC_MOUNT_DIR)/builder/build/build-collector.sh"

container/bin/collector: cmake-build/collector
	mkdir -p container/bin
	cp "$(COLLECTOR_BIN_DIR)/collector" container/bin/collector
	cp "$(COLLECTOR_BIN_DIR)/self-checks" container/bin/self-checks

.PHONY: collector
collector: container/bin/collector txt-files
	mkdir -p container/libs
	cp -r "$(CMAKE_DIR)"/THIRD_PARTY_NOTICES/* container/THIRD_PARTY_NOTICES/
	cp "$(LIBSINSP_BIN_DIR)/libsinsp-wrapper.so" container/libs/libsinsp-wrapper.so

.PHONY: build-connscrape-test
build-connscrape-test:
	docker build -f $(CURDIR)/connscrape-test/Dockerfile -t connscrape-test $(CURDIR)/connscrape-test

connscrape: build-connscrape-test
	docker rm -fv collector_connscrape || true
	docker run --rm --platform ${PLATFORM} --name collector_connscrape \
		-v "$(LIBSINSP_BIN_DIR)/libsinsp-wrapper.so:/usr/local/lib/libsinsp-wrapper.so:ro" \
		-v "$(BASE_PATH):$(SRC_MOUNT_DIR)" \
		connscrape-test "$(SRC_MOUNT_DIR)/collector/connscrape-test/connscrape-test.sh"

unittest: collector
	docker rm -fv collector_unittest || true
	docker run --rm --platform ${PLATFORM} --name collector_unittest \
		-v "$(LIBSINSP_BIN_DIR)/libsinsp-wrapper.so:/usr/local/lib/libsinsp-wrapper.so:ro" \
		-v "$(BASE_PATH):$(SRC_MOUNT_DIR)" \
		quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) $(COLLECTOR_PRE_ARGUMENTS) "$(SRC_MOUNT_DIR)/$(CMAKE_BASE_DIR)/collector/runUnitTests"

.PHONY: txt-files
txt-files:
	mkdir -p container/THIRD_PARTY_NOTICES/
	cp LICENSE-kernel-modules.txt container/
	cp NOTICE-sysdig.txt container/THIRD_PARTY_NOTICES/sysdig

.PHONY: clean
clean:
	docker rm -fv build_collector || true
	docker rm -fv container-stats benchmark collector grpc-server || true
	rm -rf falcosecurity-libs/cmake-build
	rm -rf falcosecurity-libs/falcosecurity-libs-build
	rm -rf container/bin
	rm -rf container/driver
	rm -rf container/libs
	rm -rf container/rhel/scripts
	rm -rf container/rhel/bundle.tar.gz
	rm -rf container/devel/scripts
	rm -rf container/devel/bundle.tar.gz
	rm -rf container/THIRD_PARTY_NOTICES
	rm -f container/Dockerfile.dev

.PHONY: check
check:
	git ls-files | grep -E '\.(cpp|h|cu|cuh)$$' | grep -v optionparser.h | xargs clang-format -Werror --style=file -n

.PHONY: check-staged
check-staged:
	git diff --name-only --cached --relative | grep -E '\.(cpp|h|cu|cuh)$$' | grep -v optionparser.h | xargs clang-format -Werror --style=file -n

.PHONY: format
format:
	git ls-files | grep -E '\.(cpp|h|cu|cuh)$$' | grep -v optionparser.h | xargs clang-format -Werror --style=file -i
