HDRS := $(wildcard lib/*.h) $(shell find sysdig/src/userspace -name '*.h')

SRCS := $(wildcard lib/*.cpp) collector.cpp

GENERATED_SRCS := $(GENERATED_DATA_SRCS) $(GENERATED_API_SRCS) $(GENERATED_CPP_DATA_SRCS)

BASE_PATH ?= $(CURDIR)/..

PATH ?= $(PATH):/go/bin

ifeq ($(COLLECTOR_TAG),)
	export COLLECTOR_TAG=$(shell git describe --tags --abbrev=10 --dirty)
endif

.PHONY: all
all: image integration-tests integration-test-report

.SUFFIXES:

.PHONY: image
image: build_collector container/bin/collector container/Dockerfile container/scripts/ pull-modules
	cp NOTICE-collector.txt container/
	cp COPYING.txt container/
	docker build -f container/Dockerfile -t stackrox/collector:$(COLLECTOR_TAG) container
	docker build -f container/Dockerfile_slim -t stackrox/collector:$(COLLECTOR_TAG)-slim container

build_collector: 
	@echo "Build collector builder docker image"
	docker build \
	  --cache-from stackrox/collector-builder:cache-local \
	  -t stackrox/collector-builder:cache-local \
	  "${BASE_PATH}/builder"

container/bin/collector: $(HDRS) $(SRCS) generated-srcs container/libs/libsinsp-wrapper.so
	mkdir -p container/bin
	docker rm -fv build_collector || true
	docker run \
          -v "$(CURDIR):/src:ro" \
          -v "$(CURDIR)/sysdig/src:/usr/local/sysdig:ro" \
          -v "$(CURDIR)/container/libs/libsinsp-wrapper.so:/usr/local/lib/libsinsp-wrapper.so:ro" \
          -v "$(CURDIR)/sysdig/src/driver/ppm_events_public.h:/usr/local/include/ppm_events_public.h:ro" \
          -v "$(CURDIR)/sysdig/src/driver/ppm_fillers.h:/usr/local/include/ppm_fillers.h:ro" \
          -v "$(CURDIR)/cmake-build:/build-output" \
	  --name build_collector stackrox/collector-builder:cache-local
	cp cmake-build/collector container/bin/

container/libs/libsinsp-wrapper.so: $(shell find sysdig/src/ -name '*.h' -o -name '*.cpp' -o -name '*.c')
	make -C sysdig
	mkdir -p container/libs/
	cp sysdig/build-output/userspace/libsinsp/libsinsp-wrapper.so container/libs/

.PHONY: clean
clean: clean-generated-srcs
	docker rm -fv build_collector || true
	docker rmi -f stackrox/collector:latest || true
	docker rmi -f stackrox/collector-builder:cache-local || true
	make -C sysdig clean
	rm -rf container/bin
	rm -rf container/driver
	rm -rf container/libs
	rm -f container/COPYING*.txt
	rm -f container/NOTICE.txt

.PHONY: clean-generated-srcs
clean-generated-srcs:
	rm -rf $(GENERATED_CPP_BASE_PATH)
	rm -rf $(GENERATED_GO_BASE_PATH)

build-kernel-modules:
	make -C "${BASE_PATH}/kernel-modules" build-container

prepare-src: build-kernel-modules
	rm -rf ${BASE_PATH}/kernel-modules/kobuild-tmp/
	mkdir -p ${BASE_PATH}/kernel-modules/kobuild-tmp/versions-src
	docker run --rm -i \
	  -v "${BASE_PATH}/sysdig/src:/sysdig/src" \
	  -v "${BASE_PATH}/kernel-modules/kobuild-tmp/versions-src:/output" \
	  --tmpfs /scratch:exec \
	  --env SYSDIG_DIR=/sysdig/src --env SCRATCH_DIR=/scratch --env OUTPUT_DIR=/output \
	build-kernel-modules prepare-src

module-version: prepare-src
	$(eval VERSIONS = $$(wildcard ${BASE_PATH}/kernel-modules/kobuild-tmp/versions-src/*.tgz))
	$(eval MODULE_VERSION := $(shell basename -s .tgz $(word 1,$(VERSIONS))))
	$(eval MODULE_VERSION := 7c30b6f295bae9ccf8695982687d871847dfecd12a1cfbc3edcfa93ceec6b5dc)
	@echo "MODULE VERSION is $(MODULE_VERSION)"

pull-modules: module-version
	( docker image inspect stackrox/kernel-modules:$(MODULE_VERSION) > /dev/null || docker pull stackrox/kernel-modules:$(MODULE_VERSION) )
	docker tag stackrox/kernel-modules:$(MODULE_VERSION) stackrox/kernel-modules:latest

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

.PHONY: integration-tests
integration-tests:
	@echo "+ $@"
	make -C ../integration-tests clean-deps deps
	go test -count=1 -v ../integration-tests/... 2>&1 | tee integration-test.log

.PHONY: integration-tests-ebpf
integration-tests-ebpf:
	@echo "+ $@"
	make -C ../integration-tests clean-deps deps
	COLLECTION_METHOD=ebpf go test -count=1 -v ../integration-tests/... 2>&1 | tee integration-test.log

.PHONY: integration-test-report
integration-test-report:
	@echo "+ $@"
	@cat integration-test.log | go-junit-report > integration-test-report.xml
	@echo
	@echo "Test coverage summary:"
	@grep "^coverage: " -A1 integration-test.log | grep -v -e '--' | paste -d " "  - -
	@echo
	@echo "Test pass/fail summary:"
	@grep failures integration-test-report.xml | column -t
	@echo
	@echo "`grep 'FAIL  github.com/stackrox/collector' integration-test.log | wc -l` package(s) detected with compilation or test failures."
	@-grep 'FAIL    github.com/stackrox/collector' integration-test.log || true
	@echo
	@testerror="$$(grep -e 'Unable to deploy' -e 'FAIL:' integration-test.log | wc -l)" && test $$testerror -eq 0

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

install-grpc:
	@$(BASE_PATH)/build/scripts/install-grpc-cpp-plugin.sh

.PHONY: tag
tag:
	@git describe --tags --abbrev=10 --dirty

include ../build/protogen.mk

# Generating go protos too for testing purpose
generated-srcs: install-grpc generated-proto-srcs

