HDRS := $(wildcard lib/*.h) $(shell find sysdig/src/userspace -name '*.h')

SRCS := $(wildcard lib/*.cpp) collector.cpp

GENERATED_SRCS := $(GENERATED_CPP_DATA_SRCS)

BASE_PATH ?= $(CURDIR)/..

.PHONY: all
all: image

.PHONY: image
image: container/bin/collector container/Dockerfile container/scripts/
	cp NOTICE-collector.txt container/
	cp COPYING.txt container/
	cd container && docker build -f Dockerfile -t stackrox/collector:latest .

.PHONY: build_collector
build_collector:
	( docker images --format "{{.Repository}}" | grep '^build_collector$$') || docker build -t build_collector -f Dockerfile .

container/bin/collector: $(HDRS) $(SRCS) generated-srcs container/libs/libsinsp-wrapper.so build_collector Dockerfile
	mkdir -p container/bin
	docker rm -f build_collector || true
	docker run \
	  -v $(CURDIR)/container/libs/libsinsp-wrapper.so:/usr/local/lib/libsinsp-wrapper.so \
	  -v $(CURDIR):/src \
	  -v $(CURDIR)/sysdig/src:/usr/local/sysdig \
	  -v $(CURDIR)/sysdig/src/driver/ppm_events_public.h:/usr/local/include/ppm_events_public.h \
	  --name build_collector build_collector
	cp cmake-build/collector container/bin/
	docker cp build_collector:/usr/local/lib/libcurl.so container/libs/
	docker cp build_collector:/usr/local/bin/curl container/bin/
	docker cp -L build_collector:/usr/local/lib/librdkafka.so container/libs/
	docker cp -L build_collector:/usr/local/lib/libgrpc.so container/libs/

container/libs/libsinsp-wrapper.so: $(shell find sysdig/src/ -name '*.h' -o -name '*.cpp')
	make -C sysdig
	mkdir -p container/libs/
	cp sysdig/src/build/userspace/libsinsp/libsinsp-wrapper.so container/libs/

.PHONY: clean
clean: clean-generated-srcs
	docker rm -fv build_collector || true
	docker rmi -f stackrox/collector:latest || true
	docker rmi -f build_collector:latest || true
	make -C sysdig clean
	rm -rf container/bin
	rm -rf container/driver
	rm -rf container/libs
	rm -f container/COPYING*.txt
	rm -f container/NOTICE.txt

.PHONY: clean-generated-srcs
clean-generated-srcs:
	rm -rf $(GENERATED_CPP_BASE_PATH)

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

install-grpc:
	@$(BASE_PATH)/build/scripts/install-grpc-cpp-plugin.sh

include ../build/protogen.mk

generated-srcs: install-grpc $(GENERATED_CPP_DATA_SRCS) clean-protogen-artifacts

