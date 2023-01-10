
.PHONY: build-test-container
build-test-container:
	docker build -t collector-integration-tests .

.PHONY: run
run: build-test-container
	docker run -it --rm \
		-v/tmp:/tmp \
		-v /var/run/docker.sock:/var/run/docker.sock \
		-e QUAY_USERNAME \
		-e QUAY_PASSWORD \
		collector-integration-tests pytest -s /tests
