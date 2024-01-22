# PLATFORM can be a comma separated list of platforms of the form linux/<arch>
# it is passed to buildx, which will build for each one (this is important
# for pushing multi-arch images because `buildx --push` will push a manifest list
# containing all the relevant platforms)
PLATFORM ?= linux/amd64

# path is relative to the build directory (i.e. subdirectories of integration-test/container)
COLLECTOR_QA_TAG ?= $(shell cat "../QA_TAG")
