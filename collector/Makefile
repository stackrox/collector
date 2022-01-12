BASE_PATH = ./..
include ../Makefile-constants.mk

HDRS := $(wildcard lib/*.h) $(shell find falcosecurity-libs/userspace -name '*.h')

SRCS := $(wildcard lib/*.cpp) collector.cpp

.SUFFIXES:

generated-srcs:
	docker run --rm \
		-v $(CURDIR):/collector/collector \
		-v $(CURDIR)/../rox-proto:/collector/rox-proto:ro \
		stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) make -f protogen.mk generated-proto-srcs

container/bin/collector: $(HDRS) $(SRCS) txt-files generated-srcs $(shell find falcosecurity-libs/ -name '*.h' -o -name '*.cpp' -o -name '*.c')
	mkdir -p container/bin
	mkdir -p container/libs
	docker rm -fv build_collector || true
	docker run --rm --name build_collector \
		-v "$(CURDIR):/src" \
		-v "$(CURDIR)/../falcosecurity-libs/:/falcosecurity-libs:ro" \
		-v "$(CURDIR)/cmake-build:/tmp/cmake-build" \
		-e CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		-e USE_VALGRIND=$(USE_VALGRIND) \
		-e ADDRESS_SANITIZER=$(ADDRESS_SANITIZER) \
		-e COLLECTOR_APPEND_CID=$(COLLECTOR_APPEND_CID) \
		-e DISABLE_PROFILING="true" \
		stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) /build-collector.sh
	cp cmake-build/collector container/bin/collector
	cp -r cmake-build/THIRD_PARTY_NOTICES/* container/THIRD_PARTY_NOTICES/
	cp cmake-build/EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so container/libs/libsinsp-wrapper.so

unittest:
	docker rm -fv collector_unittest || true
	docker run --rm --name collector_unittest \
		-v "$(CURDIR)/container/libs/libsinsp-wrapper.so:/usr/local/lib/libsinsp-wrapper.so:ro" \
		-v "$(CURDIR)/cmake-build:/src/cmake-build" \
		stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) $(COLLECTOR_PRE_ARGUMENTS) /src/cmake-build/runUnitTests

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
