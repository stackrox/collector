HDRS := $(shell find . -name '*.h' -not -path './container/*')
SRCS := $(shell find . -name '*.cpp' -not -path './container/*')

#determine platform
ifeq (Ubuntu, $(findstring Ubuntu, $(shell docker info)))
  PLATFORM := Ubuntu
  DOCKERFILE := Dockerfile-ubuntu
endif
ifeq (CentOS, $(findstring CentOS, $(shell docker info)))
  PLATFORM := CentOS
  DOCKERFILE := Dockerfile-centos
endif
ifeq (Red Hat, $(findstring Red Hat, $(shell docker info)))
  PLATFORM := RedHat
  DOCKERFILE := Dockerfile-rhel
endif

.PHONY: all clean container info

all: container

container: container/bin/collector container/$(DOCKERFILE) container/scripts/
	rm -rf container/sysblock
	cp NOTICE.txt container/
	cp COPYING.txt container/
	cp -r sysblock/src container/sysblock
	cd container && docker build -f $(DOCKERFILE) -t stackroxcollector/collector:latest .

container/include/KafkaClient.h:
	mkdir -p container/include
	cp sysdig/src/userspace/sysdig/KafkaClient.h container/include/KafkaClient.h

container/include/ppm_events_public.h:
	mkdir -p container/include
	cp sysdig/src/driver/ppm_events_public.h container/include/ppm_events_public.h

container/bin/collector: $(HDRS) $(SRCS) container/libs/libsysdignative.so container/include/KafkaClient.h container/include/ppm_events_public.h $(DOCKERFILE)
	mkdir -p container/bin
	mkdir -p container/libs
	docker rm -f build_collector || true
	docker build -t build_collector -f $(DOCKERFILE) .
	docker run --name build_collector build_collector
	mkdir -p container/bin
	docker cp build_collector:/src/build/collector container/bin/
	docker cp build_collector:/usr/local/lib/libcurl.so container/libs/
	docker cp build_collector:/usr/local/bin/curl container/bin/
	docker cp -L build_collector:/usr/local/lib/librdkafka.so container/libs/
	docker cp build_collector:/usr/local/lib/libprometheus-cpp.a container/libs/ || true
	docker cp build_collector:/usr/local/lib64/libprometheus-cpp.a container/libs/ || true
	docker cp build_collector:/usr/local/lib/libprotobuf.a container/libs/

container/libs/libsysdignative.so: sysdig/src/
	mkdir -p container/bin
	mkdir -p container/libs
	make -C sysdig
	docker cp build_sysdig:/src/build/userspace/sysdig/sysdig container/bin/
	docker cp build_sysdig:/src/driver container/
	# Remove generated files from container/driver
	find container/driver -type f -iname '*.cmd' -o -iname '*.o'  -o -iname '*.order' -o -iname '*.ko' | xargs rm -f
	rm -rf container/driver/.tmp_versions
	docker cp build_sysdig:/src/build/userspace/sysdig/libsysdignative.so container/libs/

.PHONY: clean
clean:
	docker rm -fv build_collector || true
	docker rmi -f stackroxcollector/collector:latest || true
	docker rmi -f build_collector:latest || true
	make -C sysdig clean
	rm -rf container/include
	rm -rf container/bin
	rm -rf container/driver
	rm -rf container/sysblock
	rm -rf container/libs
	rm -f container/COPYING*.txt
	rm -f container/NOTICE.txt

info:
	echo "platform:   " $(PLATFORM)
	echo "dockerfile: " $(DOCKERFILE)

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
