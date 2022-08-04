#!/usr/bin/env bash
set -exo pipefail

export GCLOUD_ZONE="${GCLOUD_ZONE:-us-central1-a}"
export GCLOUD_PROJECT="${GCLOUD_PROJECT:-stackrox-ci}"

createGCPVM() {
    local GCP_VM_NAME="$1"
    shift
    local GCP_IMAGE_FAMILY="$1"
    shift
    local GCP_IMAGE_PROJECT="$1"
    shift

    [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
    [ -z "$GCP_IMAGE_FAMILY" ] && echo "error: missing parameter GCP_IMAGE_FAMILY" && return 1
    [ -z "$GCP_IMAGE_PROJECT" ] && echo "error: missing parameter GCP_IMAGE_PROJECT" && return 1

    success=false
    echo "Trying zone $GCLOUD_ZONE"
    gcloud config set compute/zone "${GCLOUD_ZONE}"
    if gcloud compute instances create \
        --image-family "$GCP_IMAGE_FAMILY" \
        --image-project "$GCP_IMAGE_PROJECT" \
        --service-account=circleci-collector@stackrox-ci.iam.gserviceaccount.com \
        --machine-type e2-standard-2 \
        --labels="stackrox-ci=true,stackrox-ci-job=${JOB_NAME_SAFE},stackrox-ci-workflow=${PROW_JOB_ID}" \
        --boot-disk-size=20GB \
        "$GCP_VM_NAME"; then
        success=true
    else
        gcloud compute instances delete "$GCP_VM_NAME"
    fi

    if test ! "$success" = "true"; then
        echo "Could not boot instance."
        return 1
    fi

    gcloud compute instances add-metadata "$GCP_VM_NAME" --metadata serial-port-logging-enable=true
    gcloud compute instances describe --format json "$GCP_VM_NAME"

    #
    # Install SSH configs so we can use SSH directly instead of going
    # via gcloud compute ssh
    #
    gcloud compute config-ssh --ssh-config-file=/.ssh/config
    cat /.ssh/config || true
    echo "Instance created successfully: $GCP_VM_NAME"

    return 0
}

createGCPVMFromImage() {
    local GCP_VM_NAME="$1"
    shift
    local GCP_IMAGE_NAME="$1"
    shift
    local GCP_IMAGE_PROJECT="$1"
    shift

    [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
    [ -z "$GCP_IMAGE_NAME" ] && echo "error: missing parameter GCP_IMAGE_NAME" && return 1
    [ -z "$GCP_IMAGE_PROJECT" ] && echo "error: missing parameter GCP_IMAGE_PROJECT" && return 1

    success=false
    echo "Trying zone $GCLOUD_ZONE"
    gcloud config set compute/zone "${GCLOUD_ZONE}"
    if gcloud compute instances create \
        --image "$GCP_IMAGE_NAME" \
        --image-project "$GCP_IMAGE_PROJECT" \
        --service-account=circleci-collector@stackrox-ci.iam.gserviceaccount.com \
        --machine-type e2-standard-2 \
        --labels="stackrox-ci=true,stackrox-ci-job=${JOB_NAME_SAFE},stackrox-ci-workflow=${PROW_JOB_ID}" \
        --boot-disk-size=20GB \
        "$GCP_VM_NAME"; then
        success=true
    else
        gcloud compute instances delete "$GCP_VM_NAME"
    fi

    if test ! "$success" = "true"; then
        echo "Could not boot instance."
        return 1
    fi

    gcloud compute instances add-metadata "$GCP_VM_NAME" --metadata serial-port-logging-enable=true
    gcloud compute instances describe --format json "$GCP_VM_NAME"
    #
    # Install SSH configs so we can use SSH directly instead of going
    # via gcloud compute ssh
    #
    gcloud compute config-ssh --ssh-config-file=/.ssh/config
    cat /.ssh/config || true
    echo "Instance created successfully: $GCP_VM_NAME"

    return 0
}

installDockerOnUbuntuViaGCPSSH() {
    local GCP_VM_USER="$1"
    shift
    local GCP_VM_ADDRESS="$1"
    shift

    for _ in {1..3}; do
        if ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "(which docker || export DEBIAN_FRONTEND=noninteractive ; sudo apt update -y && sudo apt install -y docker.io )"; then
            return 0
        fi
        echo "Retrying in 5s ..."
        sleep 5
    done
    echo "Failed to install Docker after 3 attempts."
    return 1
}

installESMUpdatesOnUbuntuAndReboot() {
    local GCP_VM_USER="$1"
    shift
    local GCP_VM_ADDRESS="$1"
    shift
    for _ in {1..3}; do
        if ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "sudo apt update -y && sudo apt install -y ubuntu-advantage-tools && sudo ua attach ${UBUNTU_ESM_SUBSCRIPTION_TOKEN} && sudo apt update -y && sudo apt dist-upgrade -y"; then
            ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "sudo reboot" || true
            return 0
        fi
        echo "Retrying in 5s ..."
        sleep 5
    done
    echo "Failed to install ESM updates after 3 attempts."
    return 1
}

installFIPSOnUbuntuAndReboot() {
    local GCP_VM_USER="$1"
    shift
    local GCP_VM_ADDRESS="$1"
    shift
    for _ in {1..3}; do
        if ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "sudo ua enable --assume-yes fips"; then
            return 0
        fi
        echo "Retrying in 5s ..."
        sleep 5
    done
    echo "Failed to install FIPS after 3 attempts."
    return 1
}

rebootVM() {
    local GCP_VM_USER="$1"
    shift
    local GCP_VM_ADDRESS="$1"
    shift

    # Restart the VM.
    ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "sudo reboot" || true

    sleep 5

    # Ensure the VM is up and SSH is available.
    gcpSSHReady "$GCP_VM_USER" "$GCP_VM_ADDRESS"
}

installDockerOnRHELViaGCPSSH() {
    local GCP_VM_USER="$1"
    shift
    local GCP_VM_ADDRESS="$1"
    shift
    local GCP_IMAGE_FAMILY="$1"
    shift

    ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "sudo yum install -y yum-utils device-mapper-persistent-data lvm2"
    ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo"
    ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "sudo yum-config-manager --setopt=\"docker-ce-stable.baseurl=https://download.docker.com/linux/centos/${GCP_IMAGE_FAMILY: -1}/x86_64/stable\" --save"
    ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "sudo yum install -y docker-ce docker-ce-cli containerd.io"
    ssh "$GCP_VM_USER@$GCP_VM_ADDRESS" -- "sudo systemctl start docker"
}

setupDockerOnSUSEViaGCPSSH() {
    local GCP_VM_USER="$1"
    shift
    local GCP_VM_ADDRESS="$1"
    shift

    ssh "${GCP_VM_USER}@$GCP_VM_ADDRESS" -- "sudo systemctl start docker"
}

gcpSSHReady() {
    local GCP_VM_USER="$1"
    shift
    local GCP_VM_ADDRESS="$1"
    shift

    local retryCount=6
    for _ in $(seq 1 $retryCount); do
        ssh -o PasswordAuthentication=no -o StrictHostKeyChecking=no "${GCP_VM_USER}@${GCP_VM_ADDRESS}" -- whoami \
            && exitCode=0 && break || exitCode=$? && sleep 15
    done

    instance_id="$(gcloud compute instances describe "${GCP_VM_NAME}" --format='get(id)')"
    ssh-keygen -f "$HOME/.ssh/google_compute_known_hosts" -R "compute.${instance_id}"
    echo "Cleared existing ssh keys for compute.${instance_id}"

    return $exitCode
}

loginDockerViaGCPSSH() {
    local GCP_VM_USER="$1"
    shift
    local GCP_VM_ADDRESS="$1"
    shift
    local DOCKER_USER="$1"
    shift
    local DOCKER_PASS="$1"
    shift

    ssh "${GCP_VM_USER}@${GCP_VM_ADDRESS}" -- "sudo usermod -aG docker ${GCP_VM_USER}"
    ssh "${GCP_VM_USER}@${GCP_VM_ADDRESS}" -- "docker login -u '$DOCKER_USER' -p '$DOCKER_PASS' quay.io"
}

setupGCPVM() {
    local GCP_VM_NAME="$1"
    shift
    local GCP_VM_TYPE="$1"
    shift
    local GCP_IMAGE_FAMILY="$1"
    shift
    local GCP_IMAGE_NAME="$1"
    shift
    local GDOCKER_USER="$1"
    shift
    local GDOCKER_PASS="$1"
    shift

    if [[ ! "$GCP_VM_TYPE" =~ ^(coreos|cos|rhel|suse|suse-sap|ubuntu-os-pro|ubuntu-os|flatcar|fedora-coreos|garden-linux)$ ]]; then
        echo "Unsupported GCP_VM_TYPE: $GCP_VM_TYPE"
        exit 1
    fi

    local GCP_VM_USER="${GCLOUD_USER}"

    if [[ -z "${SSH_ADDRESS}" ]]; then
        export SSH_ADDRESS="${GCP_VM_NAME}.${GCLOUD_ZONE}.${GCLOUD_PROJECT}"
    fi

    if [[ "$GCP_VM_TYPE" == "flatcar" ]]; then
        GCP_IMAGE_PROJECT="kinvolk-public"
    elif [[ "$GCP_VM_TYPE" == "garden-linux" ]]; then
        GCP_IMAGE_PROJECT="sap-se-gcp-gardenlinux"
    else
        GCP_IMAGE_PROJECT="$GCP_VM_TYPE-cloud"
    fi

    if [[ -n "$GCP_IMAGE_NAME" && "$GCP_IMAGE_NAME" != "unset" ]]; then
        createGCPVMFromImage "$GCP_VM_NAME" "$GCP_IMAGE_NAME" "$GCP_IMAGE_PROJECT"
    else
        createGCPVM "$GCP_VM_NAME" "$GCP_IMAGE_FAMILY" "$GCP_IMAGE_PROJECT"
    fi

    if ! gcpSSHReady "$GCP_VM_USER" "$SSH_ADDRESS"; then
        echo "GCP SSH failure"
        exit 1
    fi

    if [[ "$GCP_VM_TYPE" =~ ^ubuntu-os ]]; then
        installDockerOnUbuntuViaGCPSSH "$GCP_VM_USER" "$SSH_ADDRESS"
    elif test "$GCP_VM_TYPE" = "rhel"; then
        installDockerOnRHELViaGCPSSH "$GCP_VM_USER" "$SSH_ADDRESS" "$GCP_IMAGE_FAMILY"
    elif [[ "$GCP_VM_TYPE" =~ "suse" ]]; then
        setupDockerOnSUSEViaGCPSSH "$GCP_VM_USER" "$SSH_ADDRESS"
    fi

    loginDockerViaGCPSSH "$GCP_VM_USER" "$SSH_ADDRESS" "$GDOCKER_USER" "$GDOCKER_PASS"

    if [[ "${GCP_VM_NAME}" =~ "ubuntu-1604-lts-esm" ]]; then
        installESMUpdatesOnUbuntuAndReboot "$GCP_VM_USER" "$SSH_ADDRESS"
        sleep 30
    fi
    if [[ "${GCP_VM_NAME}" =~ "ubuntu-pro-1804-lts" ]]; then
        installFIPSOnUbuntuAndReboot "$GCP_VM_USER" "$SSH_ADDRESS"
        rebootVM "$GCP_VM_USER" "$SSH_ADDRESS"
    fi
}
