BASE_PATH ?= $(CURDIR)/..
VERSION_FILE ?= $(BASE_PATH)"/ROX_VERSION"
VERSION ?= `cat $(VERSION_FILE) | xargs echo -n`
KO_GCS_PATH ?= "gs://collector-modules/aafeb6857323bbb8addf736c1fa9d93a7cdf968f2f0caca10a7b66bee29b665e"
PKG_GCS_PATH ?= "gs://kernel-package-cache"

.DEFAULT_GOAL = all

.PHONY: all
all: image

.PHONY: clean
clean:
	rm -rf container/kernel-modules
	rm -rf sysblock
	rm -rf sysdig
	rm -rf packages

.PHONY: build-crawl-container
build-crawl-container: crawl/Dockerfile crawl/kernel-crawler.py
	docker build -t kernel-crawler crawl/

.PHONY: crawl
crawl: build-crawl-container
	docker run -i kernel-crawler CentOS > supported-kernels/centos_unsorted.txt
	cat supported-kernels/centos_unsorted.txt | rev | sort | rev > supported-kernels/centos.txt
	rm supported-kernels/centos_unsorted.txt

	docker run -i kernel-crawler Ubuntu > supported-kernels/ubuntu.txt

.PHONY: build-centos
build-centos:
	docker volume rm ko-centos || true
	docker volume create ko-centos
	docker run \
		--rm \
		-i \
		-v $(CURDIR)/container/kernel-modules:/output \
		-v $(CURDIR)/packages:/packages \
		-v ko-centos:/scratch \
		build-kernel-modules \
		RedHat \
		< supported-kernels/centos.txt
	docker volume rm ko-centos

.PHONY: build-ubuntu
build-ubuntu:
	docker volume rm ko-ubuntu || true
	docker volume create ko-ubuntu
	docker run \
		--rm \
		-i \
		-v $(CURDIR)/container/kernel-modules:/output \
		-v $(CURDIR)/packages:/packages \
		-v ko-ubuntu:/scratch \
		build-kernel-modules \
		Ubuntu \
		< supported-kernels/ubuntu.txt
	docker volume rm ko-ubuntu

.PHONY: build
build: build-centos build-ubuntu

.PHONY: build-container
build-container:
	mkdir -p container/kernel-modules
	docker build -t build-kernel-modules -f build/Dockerfile .

sysdig/: $(CURDIR)/../ext/sysdig
	rm -rf ./sysdig || true
	cp -r $(CURDIR)/../ext/sysdig ./sysdig/

sysblock/: $(CURDIR)/../sysblock
	rm -rf ./sysblock/ || true
	cp -r $(CURDIR)/../sysblock ./sysblock/

# TODO: This isn't _really_ a phony target.
.PHONY: container/kernel-modules
container/kernel-modules: sysdig/ sysblock/ supported-kernels/ build/Dockerfile build/build-kos download-packages build-container build

.PHONY: image
image: container/kernel-modules
	cd container && docker build -t stackrox/kernel-modules:latest .

.PHONY: upload
upload:
	gsutil -m rsync -r container/kernel-modules/ ${KO_GCS_PATH}/${VERSION}/

.PHONY: upload-packages
upload-packages:
	gsutil -m rsync -r packages/ ${PKG_GCS_PATH}/

.PHONY: download-packages
download-packages:
	mkdir -p ./packages
	gsutil -m rsync -r ${PKG_GCS_PATH}/ ./packages
