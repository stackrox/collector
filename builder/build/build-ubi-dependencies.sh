#!/usr/bin/env bash

# This builds the collector dependencies with the assumption that a Red
# Hat subscription provides access to required RHEL 8 RPMs and that dependencies
# are resolved with submodules.

set -eux

# shellcheck source=SCRIPTDIR/../install/versions.sh
source ./builder/install/versions.sh

export WITH_RHEL8_RPMS="true"
export LICENSE_DIR="/THIRD_PARTY_NOTICES"
mkdir -p "${LICENSE_DIR}"

### Dependencies
cd third_party
../builder/install/20-googletest.sh
../builder/install/40-grpc.sh
../builder/install/50-libb64.sh
../builder/install/50-luajit.sh
../builder/install/50-prometheus.sh .
