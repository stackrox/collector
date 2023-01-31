#!/usr/bin/env bash

set -euo pipefail

# This script is meant to be run from inside a collector-builder image. It will
# create the driver for the current system, as well as Falco's sinsp-example
# binary, which is useful for quickly testing driver changes.
#
# Run the collector-builder image with the following command:
#   docker run --rm -it --entrypoint bash \
#       --privileged \
#       -v /var/run/docker.sock:/var/run/docker.sock \
#       -v /sys:/sys:ro \
#       -v /tmp:/tmp \
#       quay.io/stackrox-io/collector-builder:cache
#
# If you are running on an immutable system, create a docker volume then add
# the following arguments:
#   -v <your-volume>:/tmp/collector -e DEV_DRIVER_BUILDER=<your-volume>
#
# If you are lazy like me, you don't need to clone the collector repo before
# executing, you can simply run this inside the container:
#   curl https://raw.githubusercontent.com/stackrox/collector/master/utilities/sinsp-builder.sh | sh
#
# The script will clone the repo for you and build everything. You can also
# re-run the script without cloning if you provide the path to the repository
# in the `COLLECTOR_DIR` environment variable.
#
# If you don't want to re-run the entire script though, you can run
# `make -C "${COLLECTOR_DIR}/kernel-modules drivers"` to rebuild the drivers
# or `make -C "${COLLECTOR_DIR}/falcosecurity-libs/build sinsp-example` to
# rebuild the executable.
#
# If you want to use a driver builder other than the fc36 one, you should be
# able to do so by setting the `DEV_DRIVER_BUILDER` environment variable.
#
# For debug builds, `export CMAKE_BUILD_TYPE=Debug` should do the trick.
#
# If the collector repo is already cloned and you want to build everything for
# the branch you are working on, simply set `COLLECTOR_DIR` appropriately.
#
# If you are cloning the repo and want to use a specific branch, set
# `COLLECTOR_BRANCH`, otherwise, master will be used.

# Install docker-cli, needed for DinD
dnf config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
dnf install -y docker-ce-cli

if [[ "${COLLECTOR_DIR:-}" == "" ]]; then
    COLLECTOR_DIR=/tmp/collector

    git clone -b "${COLLECTOR_BRANCH:-master}" https://github.com/stackrox/collector "${COLLECTOR_DIR}"
    git -C "${COLLECTOR_DIR}" submodule update --init falcosecurity-libs
fi

# Build the drivers for the current system
make -C "${COLLECTOR_DIR}/kernel-modules" drivers

mkdir -p "${COLLECTOR_DIR}"/falcosecurity-libs/build
cd "${COLLECTOR_DIR}"/falcosecurity-libs/build

cmake -DUSE_BUNDLED_DEPS=OFF \
    -S"${COLLECTOR_DIR}/falcosecurity-libs" \
    -B"${COLLECTOR_DIR}/falcosecurity-libs/build"
make -j"$(nproc)" -C "${COLLECTOR_DIR}/falcosecurity-libs/build" sinsp-example

OUTPUT_DIR=/tmp/output
mkdir -p "${OUTPUT_DIR}"
for driver_zip in "${COLLECTOR_DIR}/kernel-modules/container/kernel-modules"/*.gz; do
    driver="$(basename "${driver_zip}")"
    driver="${driver%.gz}"
    gunzip -c "${driver_zip}" > "${OUTPUT_DIR}/${driver}"
done

cp "${COLLECTOR_DIR}/falcosecurity-libs/build/libsinsp/examples/sinsp-example" "${OUTPUT_DIR}/sinsp-example"

echo "All done! You should have everything you need under '${OUTPUT_DIR}'"
