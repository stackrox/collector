BASE_PATH = .
include Makefile-constants.mk

SHELL := /bin/bash

.PHONY: build-all
build-all:
	@set -eou pipefail; \
	mapfile -t container_dirs < <(ls -d */); \
	for container_dir in "$${container_dirs[@]}"; do \
		make -C "$$container_dir" build; \
	done

.PHONY: build-and-push-all
build-and-push-all:
	@set -eou pipefail; \
	mapfile -t container_dirs < <(ls -d */); \
	for container_dir in "$${container_dirs[@]}"; do \
		make -C "$$container_dir" build-and-push; \
	done
