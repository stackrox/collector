HDRS := $(wildcard lib/*.h) \
    sysdig/src/userspace/sysdig/KafkaClient.h \
    sysdig/src/driver/ppm_events_public.h

SRCS := $(wildcard lib/*.cpp) collector.cpp

.PHONY: all
all: image

.PHONY: image
image: container/bin/collector container/Dockerfile container/scripts/
	cp NOTICE-collector.txt container/
	cp NOTICE-sysblock.txt container/
	cp COPYING.txt container/
	cd container && docker build -f Dockerfile -t stackroxcollector/collector:latest .

.PHONY: build_collector
build_collector:
	( docker images --format "{{.Repository}}" | grep '^build_collector$$') || docker build -t build_collector -f Dockerfile .

container/bin/collector: $(HDRS) $(SRCS) container/libs/libsysdignative.so build_collector Dockerfile
	mkdir -p container/bin
	mkdir -p container/libs
	docker rm -f build_collector || true
	docker run \
	  -v $(CURDIR)/container/libs/libsysdignative.so:/usr/local/lib/libsysdignative.so \
	  -v $(CURDIR):/src \
	  -v $(CURDIR)/sysdig/src/userspace/sysdig/KafkaClient.h:/usr/local/include/KafkaClient.h \
	  -v $(CURDIR)/sysdig/src/driver/ppm_events_public.h:/usr/local/include/ppm_events_public.h \
	  --name build_collector build_collector
	cp cmake-build/collector container/bin/
	docker cp build_collector:/usr/local/lib/libcurl.so container/libs/
	docker cp build_collector:/usr/local/bin/curl container/bin/
	docker cp -L build_collector:/usr/local/lib/librdkafka.so container/libs/
	docker cp -L build_collector:/usr/local/lib/librdkafka++.so container/libs/

container/libs/libsysdignative.so: $(shell find sysdig -name '*.h' -o -name '*.cpp')
	mkdir -p container/libs
	make -C sysdig
	cp sysdig/src/build/userspace/sysdig/libsysdignative.so container/libs/

.PHONY: clean
clean:
	docker rm -fv build_collector || true
	docker rmi -f stackroxcollector/collector:latest || true
	docker rmi -f build_collector:latest || true
	make -C sysdig clean
	rm -rf container/bin
	rm -rf container/driver
	rm -rf container/sysblock
	rm -rf container/libs
	rm -f container/COPYING*.txt
	rm -f container/NOTICE.txt

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
			--exclude 'sysblock' \
			--exclude-from=.gitignore \
			-czvf \
			collector.tar.gz ./
