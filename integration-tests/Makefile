deps: Gopkg.toml Gopkg.lock
	@echo "+ $@"
	@# `dep check` exits with a nonzero code if there is a toml->lock mismatch.
	dep check -skip-vendor
	@# `dep ensure` can be flaky sometimes, so try rerunning it if it fails.
	dep ensure || (rm -rf .vendor-new && dep ensure)
	@touch deps

.PHONY: clean-deps
clean-deps:
	@echo "+ $@"
	rm -f deps
