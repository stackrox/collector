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
        --machine-type n1-standard-2 \
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

copySourceTarball() {
  local GCP_VM_NAME="$1"
  shift
  local SOURCE_ROOT="$1"
  shift

  [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
  [ -z "$SOURCE_ROOT" ] && echo "error: missing parameter SOURCE_ROOT dir" && return 1

  sleep 30  # give it time to boot
  buildSourceTarball "$SOURCE_ROOT"
  scpSourceTarballToGcpHost "$GCP_VM_NAME"
  return 0
}

# builds collector.tar.gz from $1 git clone (not working dir!)
buildSourceTarball() {
  local gitdir="$1"
  [ -z "$gitdir" ] && echo "error: missing parameter git source dir" && return 1
  cd /tmp
  git clone $gitdir shipdir
  rm -rf shipdir/.git
  echo $CIRCLE_BUILD_NUM > shipdir/buildnum.txt
  mkdir s2
  mv shipdir s2/collector
  cd s2
  tar cvfz collector.tar.gz collector/
  cd ..
  mv s2/collector.tar.gz .
  rm -rf s2
}

# assumes file in current working dir collector.tar.gz should be copied
# to $1 : destination instancename for GCP.
scpSourceTarballToGcpHost() {
  local GCP_VM_NAME="$1"
  gcloud compute scp collector.tar.gz "$GCP_VM_NAME":
}

# TODO: fix function name
installVariousAptDepsViaGCPSSH() {
  local GCP_VM_NAME="$1"
  gcloud compute ssh "$GCP_VM_NAME" --command "(which docker || export DEBIAN_FRONTEND=noninteractive ; sudo apt update -y && sudo apt install -y make cmake g++ gcc apt-transport-https ca-certificates curl gnupg-agent wget software-properties-common && curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add - && sudo add-apt-repository 'deb [arch=amd64] https://download.docker.com/linux/ubuntu bionic stable' && sudo apt update -y && DEBIAN_FRONTEND=noninteractive sudo apt install -y docker-ce && sudo adduser $(id -un) docker)"
}

installDockerOnUbuntuViaGCPSSH() {
  local GCP_VM_NAME="$1"
  gcloud compute ssh "$GCP_VM_NAME" --command "(which docker || export DEBIAN_FRONTEND=noninteractive ; sudo apt update -y && sudo apt install -y apt-transport-https ca-certificates curl gnupg-agent wget software-properties-common && curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add - && sudo add-apt-repository 'deb [arch=amd64] https://download.docker.com/linux/ubuntu bionic stable' && sudo apt update -y && DEBIAN_FRONTEND=noninteractive sudo apt install -y docker-ce && sudo adduser $(id -un) docker)"
}

installDockerOnRHELViaGCPSSH() {
  local GCP_VM_NAME="$1"
  # sudo yum install -y yum-utils device-mapper-persistent-data lvm2
  # sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
  # sudo yum install -y docker-ce docker-ce-cli containerd.io
  # sudo systemctl start docker
  # sudo usermod -aG docker $(whoami)

  gcloud compute ssh "$GCP_VM_NAME" --command "sudo yum install -y yum-utils device-mapper-persistent-data lvm2"
  gcloud compute ssh "$GCP_VM_NAME" --command "sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo"
  gcloud compute ssh "$GCP_VM_NAME" --command "sudo yum install -y docker-ce docker-ce-cli containerd.io"
  gcloud compute ssh "$GCP_VM_NAME" --command "sudo systemctl start docker"
  gcloud compute ssh "$GCP_VM_NAME" --command "sudo usermod -aG docker $(whoami)"
}

# parameters GCPVMName dockerUsername dockerPassword
loginDockerViaGCPSSH() {
  local GCP_VM_NAME="$1"
  shift
  local DOCKER_USER="$1"
  shift
  local DOCKER_PASS="$1"
  shift
  gcloud compute ssh "$GCP_VM_NAME" --command "docker login -u '$DOCKER_USER' -p '$DOCKER_PASS'"
}

extractSourceTarballViaGCPSSH() {
  local GCP_VM_NAME="$1"
  gcloud compute ssh "$GCP_VM_NAME" --command "tar xvpfz collector.tar.gz && rm collector.tar.gz"
}

