PLATFORM ?= linux/amd64
IMAGE_ARCH = $(word 2,$(subst /, ,$(PLATFORM)))
