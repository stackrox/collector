
# Integration Test Container Images

## General notes
Any subdirectories must contain a `Makefile` with a default and a push target. This greatly simplifies CI, since it will
automatically build all containers by calling the default target and later push it by calling `make push` on each
subdirectory.

## stackrox/benchmark-collector:phoronix
- This is preconfigured version of phoronix that runs the hackbench process workload -- the configuration XML files were created by running phoronix manually to create the batch configuration files and extracting the generated xml files.

## stackrox/benchmark-collector:container-stats
- This is a simple docker-in-docker image that emits a JSON line of cpu and mem for a subset of containers running on the host (benchmark,collector,grpc-server)
