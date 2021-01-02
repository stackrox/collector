BASE_PATH = ./..
include ../Makefile-constants.mk

HDRS := $(wildcard lib/*.h) $(shell find sysdig/src/userspace -name '*.h')

SRCS := $(wildcard lib/*.cpp) collector.cpp

.SUFFIXES:

generated-srcs:
	docker run --rm \
		-v $(CURDIR):/collector/collector \
		-v $(CURDIR)/../rox-proto:/collector/rox-proto:ro \
		stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) make -f protogen.mk generated-proto-srcs

generated-srcs-rhel:
	docker run --rm \
		-v $(CURDIR):/collector/collector \
		-v $(CURDIR)/../rox-proto:/collector/rox-proto:ro \
		stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) make -f protogen.mk generated-proto-srcs

container/bin/collector: $(HDRS) $(SRCS) txt-files generated-srcs $(shell find sysdig/src/ -name '*.h' -o -name '*.cpp' -o -name '*.c')
	mkdir -p container/bin
	mkdir -p container/libs
	docker rm -fv build_collector || true
	docker run --rm --name build_collector \
		-v "$(CURDIR):/src:ro" \
		-v "$(CURDIR)/../sysdig/:/sysdig:ro" \
		-v "$(CURDIR)/cmake-build:/tmp/cmake-build" \
		stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) /build-collector.sh
	cp cmake-build/collector container/bin/
	cp -r cmake-build/THIRD_PARTY_NOTICES/* container/THIRD_PARTY_NOTICES/
	cp cmake-build/EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so container/libs/libsinsp-wrapper.so

container/bin/collector-rhel: $(HDRS) $(SRCS) txt-files generated-srcs-rhel $(shell find sysdig/src/ -name '*.h' -o -name '*.cpp' -o -name '*.c')
	mkdir -p container/bin
	mkdir -p container/libs
	docker rm -fv build_collector_rhel || true
	docker run --rm --name build_collector_rhel \
		-v "$(CURDIR):/src:ro" \
		-v "$(CURDIR)/../sysdig/:/sysdig:ro" \
		-v "$(CURDIR)/cmake-build-rhel:/tmp/cmake-build" \
		stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) /build-collector.sh
	cp cmake-build-rhel/collector container/bin/collector.rhel
	cp -r cmake-build-rhel/THIRD_PARTY_NOTICES/* container/THIRD_PARTY_NOTICES.rhel/
	cp cmake-build-rhel/EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so container/libs/libsinsp-wrapper.so.rhel

unittest:
	docker rm -fv collector_unittest || true
	docker run --rm --name collector_unittest \
		-v "$(CURDIR)/container/libs/libsinsp-wrapper.so:/usr/local/lib/libsinsp-wrapper.so:ro" \
		-v "$(CURDIR):/src:ro" \
		-v "$(CURDIR)/cmake-build:/tmp/cmake-build" \
		-w /tmp/cmake-build \
		stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) make test

unittest-rhel:
	docker rm -fv collector_unittest_rhel || true
	docker run --rm --name collector_unittest_rhel \
		-v "$(CURDIR)/container/libs/libsinsp-wrapper.so.rhel:/usr/local/lib/libsinsp-wrapper.so:ro" \
		-v "$(CURDIR):/src:ro" \
		-v "$(CURDIR)/cmake-build-rhel:/tmp/cmake-build" \
		-w /tmp/cmake-build \
		stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) make test

.PHONY: txt-files
txt-files:
	mkdir -p container/THIRD_PARTY_NOTICES/ container/THIRD_PARTY_NOTICES.rhel/
	cp LICENSE-kernel-modules.txt container/
	cp -L notices/* container/THIRD_PARTY_NOTICES/
	cp -L notices/* container/THIRD_PARTY_NOTICES.rhel/

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
	docker rm -fv build_collector build_collector_rhel || true
	docker rm -fv container-stats benchmark collector grpc-server || true
	rm -rf sysdig/cmake-build
	rm -rf sysdig/cmake-build-rhel
	rm -rf sysdig/sysdig-build
	rm -rf cmake-build
	rm -rf cmake-build-rhel
	rm -rf container/bin
	rm -rf container/driver
	rm -rf container/libs
	rm -rf container/THIRD_PARTY_NOTICES
	rm -rf container/THIRD_PARTY_NOTICES.rhel

.PHONY: clean-generated-srcs
clean-generated-srcs:
	rm -rf $(GENERATED_CPP_BASE_PATH)

