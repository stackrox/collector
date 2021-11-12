#! /bin/bash

# This script uses a number of environment variables set up by CircleCI

set -eo pipefail

GCP_VM_USER="$(whoami)"
if [[ "$VM_TYPE" =~ "coreos" ]]; then
	GCP_VM_USER="core"
fi

KERNEL_VERSION="$(gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "${GCP_VM_USER}@${GCLOUD_INSTANCE}" --command "uname -r")"

# Remove trailing '+' from some distros.
KERNEL_VERSION="${KERNEL_VERSION%"+"}"

echo "Building dockerized drivers for ${KERNEL_VERSION}"

CONTEXT_DIR="${SOURCE_ROOT}/.."
BUNDLES_DIR="${CONTEXT_DIR}/bundles"
mkdir -p "${BUNDLES_DIR}"

# Get the bundle for the running machine
gsutil -m cp "gs://stackrox-kernel-bundles/bundle-${KERNEL_VERSION}*.tgz" "${BUNDLES_DIR}/"

docker build \
	--build-arg BRANCH="${CIRCLE_BRANCH}" \
	--build-arg REDHAT_USERNAME="${REDHAT_USERNAME}" \
	--build-arg REDHAT_PASSWORD="${REDHAT_PASSWORD}" \
	--tag kernel-builder \
	-f "${SOURCE_ROOT}/kernel-modules/dockerized/Dockerfile" \
	"${CONTEXT_DIR}/"

# Check the required module has been built
if [[ "${COLLECTION_METHOD}" == "module" ]]; then
	TEST_PATTERN="${KERNEL_VERSION}"
	if [[ "${IMAGE_FAMILY}" == "ubuntu-2004-lts" ]]; then
		TEST_PATTERN="${KERNEL_VERSION}~20\.04"
	elif [[ "${IMAGE_FAMILY}" == "ubuntu-1604-lts" ]]; then
		TEST_PATTERN="${KERNEL_VERSION}~16\.04"
	fi

	if ! docker run --rm kernel-builder 'find /kernel-modules -name "*\.ko\.gz"' | grep -q "${TEST_PATTERN}\.ko\.gz"; then
		echo >&2 "Required kernel module for ${KERNEL_VERSION} has not been built"
		exit 1
	fi
fi

# Build and tag the test image
docker build \
	--tag "${COLLECTOR_REPO}:${COLLECTOR_TAG}-dockerized" \
	--build-arg COLLECTOR_TAG="${COLLECTOR_TAG}-slim" \
	"${SOURCE_ROOT}/kernel-modules/dockerized/tests"

# Upload the image under test to the remote VM
docker save "${COLLECTOR_REPO}:${COLLECTOR_TAG}-dockerized" -o ~/image.tar
gcloud compute scp --ssh-key-file="${GCP_SSH_KEY_FILE}" ~/image.tar "${GCP_VM_USER}@${GCLOUD_INSTANCE}:/home/${GCP_VM_USER}/image.tar"
gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "${GCP_VM_USER}@${GCLOUD_INSTANCE}" --command "docker load -i '/home/${GCP_VM_USER}/image.tar' && rm -f '/home/${GCP_VM_USER}/image.tar'"
rm -f ~/image.tar
