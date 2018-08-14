KO_GCS_PATH ?= "gs://collector-modules/aafeb6857323bbb8addf736c1fa9d93a7cdf968f2f0caca10a7b66bee29b665e"
PKG_GCS_PATH ?= "gs://kernel-headers-cache"
PKG_CACHE_DIR = /packages

.DEFAULT_GOAL = all

.PHONY: all
all: image

.PHONY: clean
clean:
	rm -rf container/kernel-modules

.PHONY: build-crawl-container
build-crawl-container: crawl/Dockerfile crawl/kernel-crawler.py
	docker build -t kernel-crawler crawl/

.PHONY: crawl-centos
crawl-centos: build-crawl-container
	docker run --rm -i kernel-crawler crawl CentOS --preserve-removed-urls < supported-kernels/centos.txt > centos_urls_tmp.json
	docker run --rm -i kernel-crawler output-from-json crawled < centos_urls_tmp.json > supported-kernels/centos.txt
	docker run --rm -i kernel-crawler output-from-json removed < centos_urls_tmp.json >> supported-kernels/centos-uncrawled.txt
# The following sed removes multiple trailing newlines at the end of the file
# (since the above line will append an extra newline if there are no new uncrawled kernels).
	sed -e :a -e '/^\n*$$/{$$d;N;ba' -e '}' -i '' supported-kernels/centos-uncrawled.txt
	rm centos_urls_tmp.json

.PHONY: crawl
crawl: build-crawl-container crawl-centos
	docker run --rm -i kernel-crawler crawl CoreOS > supported-kernels/coreos.txt
	docker run --rm -i kernel-crawler crawl Ubuntu > supported-kernels/ubuntu-standard.txt
	docker run --rm -i kernel-crawler crawl Ubuntu-AWS > supported-kernels/ubuntu-aws.txt
	docker run --rm -i kernel-crawler crawl Ubuntu-Azure > supported-kernels/ubuntu-azure.txt
	docker run --rm -i kernel-crawler crawl Ubuntu-GKE > supported-kernels/ubuntu-gke.txt
	docker run --rm -i kernel-crawler crawl Ubuntu-GCP > supported-kernels/ubuntu-gcp.txt
	# kbuild tools are required for each major version of the kernel for Debian.
	echo 'http://http.us.debian.org/debian/pool/main/l/linux-tools/linux-kbuild-4.4_4.4-4~bpo8+1_amd64.deb' > supported-kernels/kops.txt
	docker run --rm -i --entrypoint python3 kernel-crawler apt-crawler.py /tmp 1>> supported-kernels/kops.txt

.PHONY: build-centos-uncrawled
build-centos-uncrawled:
	./build-modules RedHat supported-kernels/centos-uncrawled.txt

.PHONY: build-centos
build-centos:
	./build-modules RedHat supported-kernels/centos.txt

UBUNTU_SOURCES = $(shell ls supported-kernels/ubuntu-*)
supported-kernels/ubuntu.txt: $(UBUNTU_SOURCES)
	@cat $(UBUNTU_SOURCES) > supported-kernels/ubuntu.txt

.PHONY: build-ubuntu
build-ubuntu: supported-kernels/ubuntu.txt
	./build-modules Ubuntu supported-kernels/ubuntu.txt

.PHONY: build-ubuntu-aws
build-ubuntu-aws:
	./build-modules Ubuntu supported-kernels/ubuntu-aws.txt

.PHONY: build-ubuntu-azure
build-ubuntu-azure:
	./build-modules Ubuntu supported-kernels/ubuntu-azure.txt

.PHONY: build-ubuntu-gcp
build-ubuntu-gcp:
	./build-modules Ubuntu supported-kernels/ubuntu-gcp.txt

.PHONY: build-ubuntu-gke
build-ubuntu-gke:
	./build-modules Ubuntu supported-kernels/ubuntu-gke.txt

.PHONY: build-ubuntu-standard
build-ubuntu-standard:
	./build-modules Ubuntu supported-kernels/ubuntu-standard.txt

.PHONY: build-coreos
build-coreos:
	./build-modules CoreOS supported-kernels/coreos.txt

.PHONY: build-kops
build-kops:
	docker volume rm ko-kops || true
	docker volume create ko-kops
	docker run \
		--rm \
		-i \
		-v $(CURDIR)/container/kernel-modules:/output \
		-v $(CURDIR)/packages:/packages \
		-v ko-kops:/scratch \
		-v $(CURDIR)/../ext/sysdig/src:/sysdig-src \
		build-kernel-modules \
		Debian \
		< supported-kernels/kops.txt
	docker volume rm ko-kops

.PHONY: build
build: build-kops build-coreos build-centos build-centos-uncrawled build-ubuntu

.PHONY: build-container
build-container:
	docker build -t build-kernel-modules -f build/Dockerfile .

# TODO: This isn't _really_ a phony target.
.PHONY: container/kernel-modules
container/kernel-modules: supported-kernels/ build/Dockerfile build/build-kos download-package-cache build-container build-coreos-repackage build

.PHONY: image
image: container/kernel-modules
	docker cp output:/output/. container/kernel-modules/
	cd container && docker build -t stackrox/kernel-modules:latest .

.PHONY: upload-modules
upload-modules:
	gsutil -m rsync -r container/kernel-modules/ ${KO_GCS_PATH}/${VERSION}/

.PHONY: upload-package-cache
upload-package-cache:
	gsutil -m rsync -r $(PKG_CACHE_DIR)/ ${PKG_GCS_PATH}/

.PHONY: download-package-cache
download-package-cache:
	mkdir -p $(PKG_CACHE_DIR)
	gsutil -m rsync -r -d -x '.*coreos_developer_container.bin.bz2$\|.*version.txt$\' ${PKG_GCS_PATH} $(PKG_CACHE_DIR)

.PHONY: delete-package-cache
delete-package-cache:
	rm -rf $(PKG_CACHE_DIR)

.PHONY: build-coreos-repackage
build-coreos-repackage:
	docker build -t stackrox/coreos-repackage coreos-repackage

.PHONY: print-package-cache-path
print-package-cache-path:
	@echo ${PKG_GCS_PATH}
