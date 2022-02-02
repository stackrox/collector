
.PHONY: all
all:
	@docker build -t stackrox/collector-performance:base -f Dockerfile.base .
	@docker build -t stackrox/collector-performance:init -f Dockerfile.init .
	@docker build -t stackrox/collector-performance:perf -f Dockerfile.perf .
	@docker build -t stackrox/collector-performance:bpftrace -f Dockerfile.bpftrace .
	@docker build -t stackrox/collector-performance:bcc -f Dockerfile.bcc .

.PHONY: push-all
push-all:
	@docker push stackrox/collector-performance:init
	@docker push stackrox/collector-performance:perf
	@docker push stackrox/collector-performance:bpftrace
	@docker push stackrox/collector-performance:bcc
