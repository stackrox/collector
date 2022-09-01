TAG := $(shell $(MAKE) -C .. tag)

IMAGE = us.gcr.io/stackrox-collector/public-server:$(TAG)

.PHONY: all
all: image

.PHONY: image
image:
ifdef BUILD_UPLOAD_MULTIARCH_IMAGES
	docker buildx build --push --platform=linux/amd64,linux/ppc64le -t $(IMAGE) container/
else
	docker build -t $(IMAGE) container/
endif

.PHONY: run-local
run-local: image
	docker run -p 32257:32257 $(IMAGE)

.PHONY: push
push: image
	docker push $(IMAGE)
