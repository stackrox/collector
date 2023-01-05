
.PHONY: build-test-container
build-test-container:
	docker build --no-cache -t collector-integration-tests .

.PHONY: run
run: build-test-container
	docker run -it --rm -v/tmp:/tmp -v /var/run/docker.sock:/var/run/docker.sock collector-integration-tests pytest -s /tests
