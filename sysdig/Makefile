.PHONY: all clean

.DEFAULT_GOAL = all

.PHONY: build_sysdig
build_sysdig:
	( docker images --format "{{.Repository}}" | grep '^build_sysdig$$' ) || docker build -f Dockerfile -t build_sysdig .

all: build_sysdig
	echo "Building for $(PLATFORM)..."
	-docker rm -fv build_sysdig
	docker run -v /etc/os-release:/host/etc/os-release -v $(CURDIR)/src:/sysdig-src --name build_sysdig build_sysdig

clean:
	-docker rm -fv build_sysdig
	-docker rmi build_sysdig
	-rm -rf src/build
