#!/usr/bin/env bash

set -euo pipefail

# This script is meant to be run from inside a collector-builder image. It will
# create the driver for the current system, as well as Falco's sinsp-example
# binary, which is useful for quickly testing driver changes.
#
# Run the collector-builder image with the following command:
#   docker run --rm -it --entrypoint bash \
#       --privileged \
#       -e HOST_ROOT=/host \
#       -v /proc:/host/proc:ro \
#       -v /sys:/host/sys:ro \
#       -v /etc:/host/etc:ro \
#       -v /tmp:/tmp \
#       quay.io/stackrox-io/collector-builder:master
#
# If you are running on an immutable system, create a docker volume then add
# the following arguments:
#   -v <your-volume>:/tmp/collector -e DEV_SHARED_VOLUME=<your-volume>
#
# If you are lazy like me, you don't need to clone the collector repo before
# executing, you can simply run this inside the container:
#   curl https://raw.githubusercontent.com/stackrox/collector/master/utilities/sinsp-builder.sh | sh
#
# The script will clone the repo for you and build everything. You can also
# re-run the script without cloning if you provide the path to the repository
# in the `LIBS_DIR` environment variable.
#
# If you don't want to re-run the entire script though, you can run
# `make -C "${LIBS_DIR}/build sinsp-example` to rebuild the executable.
#
# For debug builds, `export CMAKE_BUILD_TYPE=Debug` should do the trick.
#
# If the libs repo is already cloned and you want to build everything for
# the branch you are working on, simply set `LIBS_DIR` appropriately.
#
# If you are cloning the repo and want to use a specific branch, set
# `LIBS_BRANCH`, otherwise, master will be used.

using_branch=1
if [[ "${LIBS_DIR:-}" == "" ]]; then
    LIBS_DIR=/tmp/falcosecurity-libs

    if ! git -C "${LIBS_DIR}" status &> /dev/null; then
        git clone -b "${LIBS_BRANCH:-master}" https://github.com/stackrox/falcosecurity-libs "${LIBS_DIR}"
    fi
elif [[ "${LIBS_BRANCH:-}" != "" ]]; then
    echo >&2 "Ignoring LIBS_BRANCH variable."
    echo >&2 "Using '${LIBS_DIR}' as is"
    using_branch=0
fi

cmake -DUSE_BUNDLED_DEPS=OFF \
    -DBUILD_LIBSCAP_MODERN_BPF=ON \
    -S"${LIBS_DIR}" \
    -B"${LIBS_DIR}/build"
    make -j"$(nproc)" -C "${LIBS_DIR}/build" sinsp-example

echo ""
echo "All done! You should have everything you need under '${LIBS_DIR}/build'"
echo ""
if ! ((using_branch)); then
    echo >&2 "'LIBS_BRANCH' variable has been ignored"
    echo >&2 "Used '${LIBS_DIR}' with no further changes"
    echo ""
fi
echo "If you plan to keep running this script run the following command"
echo "to prevent another clone of the libs repos from happening:"
echo "   export LIBS_DIR=${LIBS_DIR}"
