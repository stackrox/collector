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
  for zone in us-central1-a us-central1-b ; do
      echo "Trying zone $zone"
      gcloud config set compute/zone "${zone}"
      if gcloud compute instances create \
        --image-family "$GCP_IMAGE_FAMILY" \
        --image-project "$GCP_IMAGE_PROJECT" \
        --service-account=circleci-collector@stackrox-ci.iam.gserviceaccount.com \
        --machine-type e2-standard-2 \
        --labels="stackrox-ci=true,stackrox-ci-job=${CIRCLE_JOB},stackrox-ci-workflow=${CIRCLE_WORKFLOW_ID}" \
        --boot-disk-size=20GB \
          "$GCP_VM_NAME"
      then
          success=true
          break
      else
          gcloud compute instances delete "$GCP_VM_NAME"
      fi
  done

  if test ! "$success" = "true" ; then
    echo "Could not boot instance."
    return 1
  fi

  gcloud compute instances add-metadata "$GCP_VM_NAME" --metadata serial-port-logging-enable=true
  gcloud compute instances describe --format json "$GCP_VM_NAME"
  echo "Instance created successfully: $GCP_VM_NAME"

  return 0
}

installDockerOnUbuntuViaGCPSSH() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift
  for _ in {1..3}; do
    if gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "(which docker || export DEBIAN_FRONTEND=noninteractive ; sudo apt update -y && sudo apt install -y docker.io && sudo usermod -aG docker $(whoami) )"; then
      return 0
    fi
    echo "Retrying in 5s ..."
    sleep 5
  done
  echo "Failed to install Docker after 3 retries"
  return 1
}

installESMUpdatesOnUbuntu() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift
  for _ in {1..3}; do
    if gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo apt update -y && sudo apt install -y ubuntu-advantage-tools && sudo ua attach ${UBUNTU_ESM_SUBSCRIPTION_TOKEN} && sudo apt update -y && sudo apt dist-upgrade -y"; then
      return 0
    fi
    echo "Retrying in 5s ..."
    sleep 5
  done
  echo "Failed to install ESM updates after 3 retries"
  return 1
}


installDockerOnRHELViaGCPSSH() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_IMAGE_FAMILY="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift

  ssh-keygen -f "/home/circleci/.ssh/google_compute_known_hosts" -R "compute.$(gcloud compute instances describe $GCP_VM_NAME --format='get(id)')"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo yum install -y yum-utils device-mapper-persistent-data lvm2"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo yum-config-manager --setopt=\"docker-ce-stable.baseurl=https://download.docker.com/linux/centos/${GCP_IMAGE_FAMILY: -1}/x86_64/stable\" --save"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo yum install -y docker-ce docker-ce-cli containerd.io"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo systemctl start docker"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo usermod -aG docker $(whoami)"
}

setupDockerOnSUSEViaGCPSSH() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift

  ssh-keygen -f "/home/circleci/.ssh/google_compute_known_hosts" -R "compute.$(gcloud compute instances describe $GCP_VM_NAME --format='get(id)')"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo systemctl start docker"
}


gcpSSHReady() {
  local GCP_VM_USER="$1"
  shift
  local GCP_VM_NAME="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift

  local retryCount=6
  for _ in $(seq 1 $retryCount ); do
    gcloud compute ssh --strict-host-key-checking=no --ssh-key-file="${GCP_SSH_KEY_FILE}" "${GCP_VM_USER}@${GCP_VM_NAME}" --command "whoami" \
      && exitCode=0 && break || exitCode=$? && sleep 15
  done
  return $exitCode
}

loginDockerViaGCPSSH() {
  local GCP_VM_USER="$1"
  shift
  local GCP_VM_NAME="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift
  local DOCKER_USER="$1"
  shift
  local DOCKER_PASS="$1"
  shift

  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "${GCP_VM_USER}@${GCP_VM_NAME}" --command "docker login -u '$DOCKER_USER' -p '$DOCKER_PASS'"
}

setupGCPVM() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_VM_TYPE="$1"
  shift
  local GCP_IMAGE_FAMILY="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift

  if [[ ! "$GCP_VM_TYPE" =~ ^(coreos|cos|rhel|suse|suse-sap|ubuntu-os)$ ]]; then
    echo "Unsupported GPC_VM_TYPE: $GCP_VM_TYPE"
    exit 1
  fi

  local GCP_VM_USER="$(whoami)"
  if test "$GCP_VM_TYPE" = "coreos" ; then
    GCP_VM_USER="core"
  fi

  createGCPVM "$GCP_VM_NAME" "$GCP_IMAGE_FAMILY" "$GCP_VM_TYPE-cloud"

  if ! gcpSSHReady "$GCP_VM_USER" "$GCP_VM_NAME" "$GCP_SSH_KEY_FILE"; then
    echo "GCP SSH failure"
    exit 1
  fi

  if test "$GCP_VM_TYPE" = "ubuntu-os" ; then
    installDockerOnUbuntuViaGCPSSH "$GCP_VM_NAME" "$GCP_SSH_KEY_FILE"
    if [[ "${GCP_VM_NAME}" =~ "ubuntu-1604" ]] ; then
        installESMUpdatesOnUbuntu "$GCP_VM_NAME" "$GCP_SSH_KEY_FILE"
    fi
  elif test "$GCP_VM_TYPE" = "rhel" ; then
    installDockerOnRHELViaGCPSSH "$GCP_VM_NAME" "$GCP_IMAGE_FAMILY" "$GCP_SSH_KEY_FILE"
  elif [[ "$GCP_VM_TYPE" =~ "suse" ]] ; then
    setupDockerOnSUSEViaGCPSSH "$GCP_VM_NAME" "$GCP_SSH_KEY_FILE"
  fi

  loginDockerViaGCPSSH "$GCP_VM_USER" "$GCP_VM_NAME" "$GCP_SSH_KEY_FILE" "$GDOCKER_USER" "$GDOCKER_PASS"
}
