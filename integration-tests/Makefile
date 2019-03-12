deps: Gopkg.toml Gopkg.lock
	@echo "+ $@"
	@# `dep check` exits with a nonzero code if there is a toml->lock mismatch.
	dep check -skip-vendor
	@# `dep ensure` can be flaky sometimes, so try rerunning it if it fails.
	dep ensure || (ls -F vendor && rm -rf .vendor-new /go/pkg/dep/sources/https---go.googlesource.com-sys && dep ensure)
	@touch deps

.PHONY: clean-deps
clean-deps:
	@echo "+ $@"
	rm -f deps
	sudo rm -f /tmp/collector-test.db
