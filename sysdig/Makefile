.PHONY: all clean

.DEFAULT_GOAL = all

.PHONY: build_sysdig
build_sysdig:
	docker build --cache-from stackrox/collector-builder:sysdig -t stackrox/collector-builder:sysdig build/

all: build_sysdig
	echo "Building for $(PLATFORM)..."
	-docker rm -fv build_sysdig
	docker run -v /etc/os-release:/host/etc/os-release -v $(CURDIR)/src:/sysdig-src --name build_sysdig stackrox/collector-builder:sysdig

clean:
	-docker rm -fv build_sysdig
	-rm -rf src/build
