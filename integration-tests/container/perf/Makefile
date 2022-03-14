
.PHONY: all
all:
	@docker build -t stackrox/collector-performance:init --target init -f Dockerfile .
	@docker build -t stackrox/collector-performance:perf --target perf -f Dockerfile .
	@docker build -t stackrox/collector-performance:bcc --target bcc -f Dockerfile .
	@docker build -t stackrox/collector-performance:bpftrace -f Dockerfile.bpftrace .

.PHONY: push-all
push-all:
	@docker push stackrox/collector-performance:init
	@docker push stackrox/collector-performance:perf
	@docker push stackrox/collector-performance:bpftrace
	@docker push stackrox/collector-performance:bcc
