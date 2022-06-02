BASE_PATH = $(realpath ./..)
include ../Makefile-constants.mk

CMAKE_BASE_DIR = cmake-build
CMAKE_DIR= $(BASE_PATH)/$(CMAKE_BASE_DIR)
COLLECTOR_BIN_DIR = $(CMAKE_DIR)/collector
LIBSINSP_BIN_DIR = $(CMAKE_DIR)/collector/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp
SRC_MOUNT_DIR = /tmp/collector

HDRS := $(wildcard lib/*.h) $(shell find falcosecurity-libs/userspace -name '*.h')

SRCS := $(wildcard lib/*.cpp) collector.cpp

.SUFFIXES:

generated-srcs: $(shell find ../rox-proto -name '*.proto')
	docker run --rm \
		-v $(CURDIR):/collector/collector \
		-v $(CURDIR)/../rox-proto:/collector/rox-proto:ro \
		-v $(CURDIR)/../third_party/googleapis:/collector/googleapis:ro \
		-e BASE_PATH="/collector" \
		quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) make -f protogen.mk SKIP_GOOGLEAPI_FETCH=1 generated-proto-srcs

cmake-build/collector: $(HDRS) $(SRCS) txt-files generated-srcs $(shell find falcosecurity-libs/ -name '*.h' -o -name '*.cpp' -o -name '*.c')
	docker rm -fv build_collector || true
	docker run --rm --name build_collector \
		-v "$(BASE_PATH):$(SRC_MOUNT_DIR)" \
		-e SRC_ROOT_DIR="$(SRC_MOUNT_DIR)" \
		-e CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		-e USE_VALGRIND=$(USE_VALGRIND) \
		-e ADDRESS_SANITIZER=$(ADDRESS_SANITIZER) \
		-e COLLECTOR_APPEND_CID=$(COLLECTOR_APPEND_CID) \
		-e DISABLE_PROFILING="true" \
		quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) /build-collector.sh

container/bin/collector: cmake-build/collector
	mkdir -p container/bin
	mkdir -p container/libs
	cp "$(COLLECTOR_BIN_DIR)/collector" container/bin/collector
	cp -r "$(CMAKE_DIR)"/THIRD_PARTY_NOTICES/* container/THIRD_PARTY_NOTICES/
	cp "$(LIBSINSP_BIN_DIR)/libsinsp-wrapper.so" container/libs/libsinsp-wrapper.so

unittest:
	docker rm -fv collector_unittest || true
	docker run --rm --name collector_unittest \
		-v "$(LIBSINSP_BIN_DIR)/libsinsp-wrapper.so:/usr/local/lib/libsinsp-wrapper.so:ro" \
		-v "$(BASE_PATH):$(SRC_MOUNT_DIR)" \
		quay.io/stackrox-io/collector-builder:$(COLLECTOR_BUILDER_TAG) $(COLLECTOR_PRE_ARGUMENTS) "$(SRC_MOUNT_DIR)/$(CMAKE_BASE_DIR)/collector/runUnitTests"

.PHONY: txt-files
txt-files:
	mkdir -p container/THIRD_PARTY_NOTICES/
	cp LICENSE-kernel-modules.txt container/
	cp NOTICE-sysdig.txt container/THIRD_PARTY_NOTICES/sysdig

.PHONY: distribution
distribution: clean
# Remove user-specific files but archive the rest of the repository for easy open-source distribution.
# Makefile has references to StackRox. If you remove it from --exclude, first clean it up.
	tar --dereference \
			--exclude '.idea' \
			--exclude '*.iml' \
			--exclude '*.yml' \
			--exclude '.git' \
			--exclude '.DS_Store' \
			--exclude 'Makefile' \
			--exclude-from=.gitignore \
			-czvf \
			collector.tar.gz ./

GENERATED_CPP_BASE_PATH := $(CURDIR)/generated

ROX_URL := git@github.com:stackrox/rox.git
ROX_WORKTREE := $(BASE_PATH)/.rox
ROX_GITDIR := $(ROX_WORKTREE)/.git

.PHONY: pull-protos
pull-protos:
	@echo Fetching protobufs from rox ...
	@test -d $(ROX_WORKTREE) || git clone $(ROX_URL) $(ROX_WORKTREE)
	@test $(shell git --git-dir $(ROX_GITDIR) remote get-url origin) == $(ROX_URL) || \
		( echo >&2 "Your .rox git repo is not setup correctly (pointing to origin '$(shell git --git-dir $(ROX_GITDIR) remote get-url origin)')" ; exit 1 ; )
	@git --git-dir $(ROX_GITDIR) fetch && git --git-dir $(ROX_GITDIR) --work-tree $(ROX_WORKTREE) checkout origin/master
	@rm -rf $(BASE_PATH)/rox-proto/
	@cp -r $(ROX_WORKTREE)/proto/ $(BASE_PATH)/rox-proto
	@git --git-dir $(BASE_PATH)/.git --work-tree $(BASE_PATH) add $(BASE_PATH)/rox-proto/
	@echo "Copied protos to $(BASE_PATH)/rox-proto and added to git index"

.PHONY: clean
clean: clean-generated-srcs
	docker rm -fv build_collector || true
	docker rm -fv container-stats benchmark collector grpc-server || true
	rm -rf falcosecurity-libs/cmake-build
	rm -rf falcosecurity-libs/falcosecurity-libs-build
	rm -rf cmake-build
	rm -rf container/bin
	rm -rf container/driver
	rm -rf container/libs
	rm -rf container/rhel/scripts
	rm -rf container/rhel/bundle.tar.gz
	rm -rf container/devel/scripts
	rm -rf container/devel/bundle.tar.gz
	rm -rf container/THIRD_PARTY_NOTICES

.PHONY: clean-generated-srcs
clean-generated-srcs:
	rm -rf $(GENERATED_CPP_BASE_PATH)

.PHONY: check
check:
	git ls-files | grep -E '\.(cpp|h|cu|cuh)$$' | grep -v optionparser.h | xargs clang-format -Werror --style=file -n

.PHONY: check-staged
check-staged:
	git diff --name-only --cached --relative | grep -E '\.(cpp|h|cu|cuh)$$' | grep -v optionparser.h | xargs clang-format -Werror --style=file -n

.PHONY: format
format:
	git ls-files | grep -E '\.(cpp|h|cu|cuh)$$' | grep -v optionparser.h | xargs clang-format -Werror --style=file -i
