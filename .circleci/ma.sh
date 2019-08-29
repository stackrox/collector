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

  local REGION=us-central1

  local zone
  #zones=$(gcloud compute zones list --filter="region=$REGION" | grep UP | cut -f1 -d' ')
  success=false
  for zone in us-central1-a us-central1-b ; do
      echo "Trying zone $zone"
      gcloud config set compute/zone "${zone}"
      if gcloud compute instances create \
        --image-family $GCP_IMAGE_FAMILY \
        --image-project $GCP_IMAGE_PROJECT \
        --service-account=circleci-collector@stackrox-ci.iam.gserviceaccount.com \
        --machine-type n1-standard-4 \
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

createGCPVMUbuntu() {
  local GCP_VM_NAME="$1"

  [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
  createGCPVM $GCP_VM_NAME ubuntu-1804-lts ubuntu-os-cloud
}

createGCPVMCOS() {
  local GCP_VM_NAME="$1"

  [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
  createGCPVM $GCP_VM_NAME cos-stable cos-cloud
}

createGCPVMRHEL() {
  local GCP_VM_NAME="$1"

  [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
  createGCPVM $GCP_VM_NAME rhel-7 rhel-cloud
}

createGCPVMCoreOS() {
  local GCP_VM_NAME="$1"

  [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
  createGCPVM $GCP_VM_NAME coreos-stable coreos-cloud
}

installDockerOnUbuntuViaGCPSSH() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "(which docker || export DEBIAN_FRONTEND=noninteractive ; sudo apt update -y && sudo apt install -y apt-transport-https ca-certificates curl gnupg-agent wget software-properties-common && curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add - && sudo add-apt-repository 'deb [arch=amd64] https://download.docker.com/linux/ubuntu bionic stable' && sudo apt update -y && DEBIAN_FRONTEND=noninteractive sudo apt install -y docker-ce && sudo adduser $(id -un) docker)"
}

installDockerOnRHELViaGCPSSH() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift

  # sudo yum install -y yum-utils device-mapper-persistent-data lvm2
  # sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
  # sudo yum install -y docker-ce docker-ce-cli containerd.io
  # sudo systemctl start docker
  # sudo usermod -aG docker $(whoami)

  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo yum install -y yum-utils device-mapper-persistent-data lvm2"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo yum install -y docker-ce docker-ce-cli containerd.io"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo systemctl start docker"
  gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "$GCP_VM_NAME" --command "sudo usermod -aG docker $(whoami)"
}

gcpSSHReady() {
  local GCP_VM_USER="$1"
  shift
  local GCP_VM_NAME="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift

  local retryCount=5
  for i in $(seq 1 $retryCount ); do
    gcloud compute ssh --ssh-key-file="${GCP_SSH_KEY_FILE}" "${GCP_VM_USER}@${GCP_VM_NAME}" --command "whoami" \
      && exitCode=0 && break || exitCode=$? && sleep 10
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
