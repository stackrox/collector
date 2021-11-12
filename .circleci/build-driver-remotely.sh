#! /bin/bash

function vm_exec {
	gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "${GCP_VM_USER}@${GCLOUD_INSTANCE}" --command "$1"
}

GCP_VM_USER="$(whoami)"
if [[ "$VM_TYPE" =~ "coreos" ]]; then
	GCP_VM_USER="core"
fi

KERNEL_VERSION="$(vm_exec "uname -r")"

# Remove trailing '+' from some distros.
KERNEL_VERSION="${KERNEL_VERSION%"+"}"

echo "Building dockerized drivers for ${KERNEL_VERSION}"

BUNDLES_DIR="${HOME}/bundles"
mkdir -p "${BUNDLES_DIR}"

# Get the bundle for the running machine
gsutil -m cp "gs://stackrox-kernel-bundles/bundle-${KERNEL_VERSION}*.tgz" "${BUNDLES_DIR}"

# tar the collector repo in order to speed up transfer to the remote VM
tar -czf ~/collector.tar.gz -C ~/workspace/go/src/github.com/stackrox/ collector/

# Upload bundles and collector repo to the remote VM
vm_exec "mkdir -p '/home/${GCP_VM_USER}/ctx/bundles'"
gcloud compute scp --ssh-key-file="${GCP_SSH_KEY_FILE}" "${BUNDLES_DIR}/*" "${GCP_VM_USER}@${GCLOUD_INSTANCE}:/home/${GCP_VM_USER}/ctx/bundles"
gcloud compute scp --ssh-key-file="${GCP_SSH_KEY_FILE}" ~/collector.tar.gz "${GCP_VM_USER}@${GCLOUD_INSTANCE}:/home/${GCP_VM_USER}/ctx/"

# Decompress the collector repo
vm_exec "cd '/home/${GCP_VM_USER}/ctx' && tar xzf collector.tar.gz && rm -f collector.tar.gz"

# Create the test image on the remote machine
# Build the probes
if [[ "$VM_TYPE" =~ "coreos" ]]; then
	vm_exec "sudo docker build --build-arg BRANCH='${CIRCLE_BRANCH}' --build-arg USER='${REDHAT_USERNAME}' --build-arg PASS='${REDHAT_PASSWORD}' --tag kernel-builder -f '/home/${GCP_VM_USER}/ctx/collector/kernel-modules/dockerized/Dockerfile' '/home/${GCP_VM_USER}/ctx'"
else
	vm_exec "docker build --build-arg BRANCH='${CIRCLE_BRANCH}' --build-arg USER='${REDHAT_USERNAME}' --build-arg PASS='${REDHAT_PASSWORD}' --tag kernel-builder -f '/home/${GCP_VM_USER}/ctx/collector/kernel-modules/dockerized/Dockerfile' '/home/${GCP_VM_USER}/ctx'"
fi
