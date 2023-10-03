BASE_PATH = .
include Makefile-constants.mk

SHELL := /bin/bash

TARGETS := build build-and-push
DIRS := $(wildcard ./*/)

$(TARGETS): $(DIRS)
$(DIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TARGETS) $(DIRS)
