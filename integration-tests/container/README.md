
# Integration Test Container Images

## General notes
Any subdirectories must contain a `Makefile` with default, `build-and-push`,
and `manifest` targets. The build must support multi-arch images, and should
tag each build with the architecture used to build it. Similarly, the manifest
target should construct and push a docker manifest containing the relevant
architectures.

All images built in these subdirectories must also be tagged with
`COLLECTOR_QA_TAG`, which is populated with the contents of the `QA_TAG` file.

When changes are made to any of the images, the QA_TAG must be bumped using the
following strategy:

1. breaking changes (including new images) -> major version change
2. modifications to existing images that do not fall under (1) -> minor version
  change
3. bug fixes -> patch version change.

The intention of this version number is to ensure consistency across master
testing and release testing, whereby releases are pegged to a specific QA image
version.

To build all images run

`make build`

in this directory

To build and push all images run

`make build-and-push`

in this directory

## stackrox/benchmark-collector:phoronix
- This is preconfigured version of phoronix that runs the hackbench process workload -- the configuration XML files were created by running phoronix manually to create the batch configuration files and extracting the generated xml files.

## stackrox/benchmark-collector:container-stats
- This is a simple docker-in-docker image that emits a JSON line of cpu and mem for a subset of containers running on the host (benchmark,collector,grpc-server)
