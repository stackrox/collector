#! /bin/bash

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

# Check the required module has been built
if [[ "${COLLECTION_METHOD}" == "module" ]]; then
	test_pattern="${KERNEL_VERSION}"
	if [[ "<< parameters.image_family >>" == "ubuntu-2004-lts" ]]; then
		test_pattern="${KERNEL_VERSION}~20\.04"
	elif [[ "<< parameters.image_family >>" == "ubuntu-1604-lts" ]]; then
		test_pattern="${KERNEL_VERSION}~16\.04"
	fi

	echo "${test_pattern}"
	if ! gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "${GCP_VM_USER}@${GCLOUD_INSTANCE}" --command "docker run --rm kernel-builder 'find /kernel-modules -name \"*\.ko\.gz\"'" | grep -q "${test_pattern}\.ko\.gz"; then
		echo >&2 "Required kernel module for ${KERNEL_VERSION} has not been built"
		exit 1
	fi
fi

# Build and tag the test image
docker build \
	--tag "${COLLECTOR_REPO}:${CIRCLE_BUILD_NUM}" \
	--build-arg COLLECTOR_TAG="${COLLECTOR_TAG}-slim" \
	/home/${GCP_VM_USER}/ctx/collector/kernel-modules/dockerized/tests

docker push ${COLLECTOR_REPO}:${CIRCLE_BUILD_NUM}"
