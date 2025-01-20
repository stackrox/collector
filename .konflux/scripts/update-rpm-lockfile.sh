#!/bin/bash

################################################
# This script updates the RPM lockfile based on
# the information in rpms.in.yaml.
# If new RPMs are installed in the images, add
# them to rpms.in.yaml and re-run this script.
#
# Usage: .konflux/scripts/update-rpm-lockfile.sh
################################################

set -euo pipefail

RPM_LOCKFILE_VERSION="v0.13.2"
BASE_IMAGE="registry.access.redhat.com/ubi8-minimal:latest"

LOCAL_DIR="$(dirname "${BASH_SOURCE[0]}")/rpm-prefetching"
RPM_LOCKFILE_RUNNER_IMAGE="localhost/rpm-lockfile-runner:latest"

# fetch_ubi_repo_definitions() {
#   podman run "${BASE_IMAGE}" cat /etc/yum.repos.d/ubi.repo > "${LOCAL_DIR}/ubi.repo"
# }

build_rpm_lockfile_runner_image() {
  curl "https://raw.githubusercontent.com/konflux-ci/rpm-lockfile-prototype/refs/tags/${RPM_LOCKFILE_VERSION}/Containerfile" \
    | podman build -t "${RPM_LOCKFILE_RUNNER_IMAGE}" \
      --build-arg GIT_REF=tags/${RPM_LOCKFILE_VERSION} -
}

run_rpm_lockfile_runner() {
  local container_dir=/work
  podman run --rm -v "$(pwd)/${LOCAL_DIR}:${container_dir}" \
    "${RPM_LOCKFILE_RUNNER_IMAGE}" \
    --outfile=${container_dir}/rpms.lock.yaml \
    ${container_dir}/rpms.in.yaml
}

# fetch_ubi_repo_definitions
build_rpm_lockfile_runner_image
run_rpm_lockfile_runner
