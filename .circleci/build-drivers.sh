#! /bin/bash

set -eo pipefail

GCP_VM_USER="$(whoami)"
if [[ "$VM_TYPE" =~ "coreos" ]]; then
	GCP_VM_USER="core"
fi

KERNEL_VERSION="$(gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "${GCP_VM_USER}@${GCLOUD_INSTANCE}" --command "uname -r")"

# Remove trailing '+' from some distros.
KERNEL_VERSION="${KERNEL_VERSION%"+"}"

echo "Building dockerized drivers for ${KERNEL_VERSION}"

BUNDLES_DIR="${HOME}/bundles"
mkdir -p "${BUNDLES_DIR}"

# Get the bundle for the running machine
gsutil -m cp "gs://stackrox-kernel-bundles/bundle-${KERNEL_VERSION}*.tgz" "${BUNDLES_DIR}"

docker build \
	--build-arg BRANCH="${CIRCLE_BRANCH}" \
	--build-arg REDHAT_USERNAME="${REDHAT_USERNAME}" \
	--build-arg REDHAT_PASSWORD="${REDHAT_PASSWORD}" \
	--tag kernel-builder \
	-f "/home/${GCP_VM_USER}/ctx/collector/kernel-modules/dockerized/Dockerfile" \
	"/home/${GCP_VM_USER}/ctx"
