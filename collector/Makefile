BASE_PATH = $(realpath ./..)
include ../Makefile-constants.mk

NPROCS ?= $(shell nproc)

CMAKE_BASE_DIR = cmake-build
CMAKE_DIR= $(BASE_PATH)/$(CMAKE_BASE_DIR)
SRC_MOUNT_DIR = /tmp/collector

.SUFFIXES:

.PHONY: build-connscrape-test
build-connscrape-test:
	docker build -f $(CURDIR)/connscrape-test/Dockerfile -t connscrape-test $(CURDIR)/connscrape-test

connscrape: build-connscrape-test
	docker rm -fv collector_connscrape || true
	docker run --rm --platform ${PLATFORM} --name collector_connscrape \
		-v "$(BASE_PATH):$(SRC_MOUNT_DIR)" \
		connscrape-test "$(SRC_MOUNT_DIR)/collector/connscrape-test/connscrape-test.sh"

unittest:
	docker exec $(COLLECTOR_BUILDER_NAME) \
		ctest -V --test-dir $(CMAKE_DIR)

.PHONY: txt-files
txt-files:
	mkdir -p container/THIRD_PARTY_NOTICES/
	cp NOTICE-sysdig.txt container/THIRD_PARTY_NOTICES/sysdig

.PHONY: clean
clean:
	rm -rf container/LICENSE-kernel-modules.txt
	rm -rf container/bin
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
