BASE_PATH ?= $(CURDIR)/..
VERSION_FILE ?= $(BASE_PATH)"/ROX_VERSION"
VERSION ?= `cat $(VERSION_FILE) | xargs echo -n`
KO_GCS_PATH ?= "gs://collector-modules/aafeb6857323bbb8addf736c1fa9d93a7cdf968f2f0caca10a7b66bee29b665e"

.DEFAULT_GOAL = all

.PHONY: all
all: image

.PHONY: clean
clean:
	rm -rf container/kernel-modules
	rm -rf sysblock
	rm -rf sysdig

.PHONY: build-crawl-container
build-crawl-container: crawl/Dockerfile crawl/kernel-crawler.py
	docker build -t kernel-crawler crawl/

.PHONY: crawl
crawl: build-crawl-container
	docker run -i kernel-crawler Debian > supported-kernels/debian.txt

	docker run -i kernel-crawler CentOS > supported-kernels/centos_unsorted.txt
	sort supported-kernels/centos_unsorted.txt > supported-kernels/centos.txt
	rm supported-kernels/centos_unsorted.txt

	docker run -i kernel-crawler Ubuntu > supported-kernels/ubuntu.txt

# TODO: This isn't _really_ a phony target.
.PHONY: container/kernel-modules
container/kernel-modules: $(CURDIR)/../ext/sysdig/ $(CURDIR)/../sysblock/ supported-kernels/ build/Dockerfile build/build-kos
	mkdir -p container/kernel-modules
	rm -rf ./sysdig/ || true
	rm -rf ./sysblock/ || true
	cp -r $(CURDIR)/../ext/sysdig ./sysdig/
	cp -r $(CURDIR)/../sysblock ./sysblock/
	docker build -t build-kernel-modules -f build/Dockerfile .
	docker run \
		--rm \
		-i \
		-v $(CURDIR)/container/kernel-modules:/output \
		build-kernel-modules \
		Debian \
		< supported-kernels/debian.txt
	docker run \
		--rm \
		-i \
		-v $(CURDIR)/container/kernel-modules:/output \
		build-kernel-modules \
		RedHat \
		< supported-kernels/centos.txt
	docker run \
		--rm \
		-i \
		-v $(CURDIR)/container/kernel-modules:/output \
		build-kernel-modules \
		Ubuntu \
		< supported-kernels/ubuntu.txt

.PHONY: image
image: container/kernel-modules
	cd container && docker build -t stackrox/kernel-modules:latest .

.PHONY: upload
upload:
	gsutil -m cp -r container/kernel-modules/* ${KO_GCS_PATH}/${VERSION}/
