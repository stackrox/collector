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

container/bin/collector: $(HDRS) $(SRCS) generated-srcs container/libs/libsinsp-wrapper.so
	mkdir -p container/bin
	docker rm -fv build_collector || true
	docker run --rm \
          -v "$(CURDIR):/src:ro" \
          -v "$(CURDIR)/sysdig/src:/usr/local/sysdig:ro" \
          -v "$(CURDIR)/container/libs/libsinsp-wrapper.so:/usr/local/lib/libsinsp-wrapper.so:ro" \
          -v "$(CURDIR)/sysdig/src/driver/ppm_events_public.h:/usr/local/include/ppm_events_public.h:ro" \
          -v "$(CURDIR)/sysdig/src/driver/ppm_fillers.h:/usr/local/include/ppm_fillers.h:ro" \
          -v "$(CURDIR)/cmake-build:/build-output" \
	  --name build_collector stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) /build-collector.sh
	cp cmake-build/collector container/bin/

container/bin/collector-rhel: $(HDRS) $(SRCS) generated-srcs-rhel container/libs/libsinsp-wrapper.so.rhel
	mkdir -p container/bin
	docker rm -fv build_collector_rhel || true
	docker run --rm \
          -v "$(CURDIR):/src:ro" \
          -v "$(CURDIR)/sysdig/src:/usr/local/sysdig:ro" \
          -v "$(CURDIR)/container/libs/libsinsp-wrapper.so.rhel:/usr/local/lib/libsinsp-wrapper.so:ro" \
          -v "$(CURDIR)/sysdig/src/driver/ppm_events_public.h:/usr/local/include/ppm_events_public.h:ro" \
          -v "$(CURDIR)/sysdig/src/driver/ppm_fillers.h:/usr/local/include/ppm_fillers.h:ro" \
          -v "$(CURDIR)/cmake-build-rhel:/build-output" \
	  --name build_collector_rhel stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) /build-collector.sh
	cp cmake-build-rhel/collector container/bin/collector.rhel

unittest:
	docker rm -fv collector_unittest || true
	docker run --rm \
	  -v "$(CURDIR)/container/libs/libsinsp-wrapper.so:/usr/local/lib/libsinsp-wrapper.so:ro" \
          -v "$(CURDIR)/cmake-build:/build-output" \
	  --name collector_unittest \
	  stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) /build-output/runUnitTests

unittest-rhel:
	docker rm -fv collector_unittest_rhel || true
	docker run --rm \
	  -v "$(CURDIR)/container/libs/libsinsp-wrapper.so.rhel:/usr/local/lib/libsinsp-wrapper.so:ro" \
          -v "$(CURDIR)/cmake-build-rhel:/build-output" \
	  --name collector_unittest_rhel \
	  stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) /build-output/runUnitTests

container/libs/libsinsp-wrapper.so: $(shell find sysdig/src/ -name '*.h' -o -name '*.cpp' -o -name '*.c')
	docker rm -fv build_sysdig || true
	docker run --rm \
	  -v /etc/os-release:/host/etc/os-release \
	  -v $(CURDIR)/sysdig/src:/sysdig-src \
	  -v $(CURDIR)/sysdig/cmake-build:/sysdig-build \
	  --name build_sysdig stackrox/collector-builder:$(COLLECTOR_BUILDER_TAG) /build-sysdig.sh
	mkdir -p container/libs/
	cp sysdig/cmake-build/userspace/libsinsp/libsinsp-wrapper.so container/libs/libsinsp-wrapper.so

container/libs/libsinsp-wrapper.so.rhel: $(shell find sysdig/src/ -name '*.h' -o -name '*.cpp' -o -name '*.c')
	docker rm -fv build_sysdig_rhel || true
	docker run --rm \
	  -v /etc/os-release:/host/etc/os-release \
	  -v $(CURDIR)/sysdig/src:/sysdig-src \
	  -v $(CURDIR)/sysdig/cmake-build-rhel:/sysdig-build \
	  --name build_sysdig_rhel stackrox/collector-builder:rhel-$(COLLECTOR_BUILDER_TAG) /build-sysdig.sh
	mkdir -p container/libs/
	cp sysdig/cmake-build-rhel/userspace/libsinsp/libsinsp-wrapper.so container/libs/libsinsp-wrapper.so.rhel

.PHONY: txt-files
txt-files:
	cp NOTICE-collector.txt container/
	cp COPYING.txt container/


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
	docker rm -fv build_sysdig build_sysdig_rhel || true
	docker rm -fv container-stats benchmark collector grpc-server || true
	rm -rf sysdig/cmake-build
	rm -rf sysdig/cmake-build-rhel
	rm -rf sysdig/sysdig-build
	rm -rf cmake-build
	rm -rf cmake-build-rhel
	rm -rf container/bin
	rm -rf container/driver
	rm -rf container/libs
	rm -f container/COPYING*.txt
	rm -f container/NOTICE.txt

.PHONY: clean-generated-srcs
clean-generated-srcs:
	rm -rf $(GENERATED_CPP_BASE_PATH)

