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
  DOCKERFILE := Dockerfile-centos
endif

.PHONY: all clean container info

all: container

container: container/bin/collector container/$(DOCKERFILE) container/scripts/
	cd container && docker build -f $(DOCKERFILE) -t stackroxcollector/collector:latest .

container/include/KafkaClient.h:
	mkdir -p container/include
	cp ../ext/sysdig/src/userspace/sysdig/KafkaClient.h container/include/KafkaClient.h

container/include/ppm_events_public.h:
	mkdir -p container/include
	cp ../ext/sysdig/src/driver/ppm_events_public.h container/include/ppm_events_public.h

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

container/libs/libsysdignative.so: ../ext/sysdig/src/
	mkdir -p container/bin
	mkdir -p container/libs
	make -C ../ext/sysdig
	docker cp build_sysdig:/src/build/userspace/sysdig/sysdig container/bin/
	docker cp build_sysdig:/src/driver container/
	docker cp build_sysdig:/src/build/userspace/sysdig/libsysdignative.so container/libs/
	docker cp build_sysdig:/src/driver/sysdig-probe.ko container/bin/

clean:
	docker rm -fv build_collector || true
	docker rmi -f stackroxcollector/collector:latest || true
	docker rmi -f build_collector:latest || true
	make -C ../ext/sysdig clean
	rm -rf container/include
	rm -rf container/bin
	rm -rf container/driver
	rm -rf container/libs

info:
	echo "platform:   " $(PLATFORM)
	echo "dockerfile: " $(DOCKERFILE)
