createGCPVMUbuntu() {
  local GCP_VM_NAME="$1"
  shift
  local SOURCE_ROOT="$1"
  shift
  [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
  [ -z "$SOURCE_ROOT" ] && echo "error: missing parameter SOURCE_ROOT dir" && return 1

  buildSourceTarball "$SOURCE_ROOT"
  scpSourceTarballToGcpHost "$GCP_VM_NAME"
  return 0
}

# builds collector.tar.gz from working dir
buildSourceTarballWorkingDir() {
  local gitdir="$1"
  [ -z "$gitdir" ] && echo "error: missing parameter git source dir" && return 1
  pushd $gitdir/..
  if test -d ~/workspace -a -f ~/workspace/shared-env ; then
    cp ~/workspace/shared-env collector
  else
    touch shipdir/shared-env
  fi
  tar cvfz /tmp/collector.tar.gz collector/
  popd
  mv /tmp/collector.tar.gz .
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
  gcloud compute ssh "$GCP_VM_NAME" --command "tar xvpfz collector.tar.gz && rm collector.tar.gz && echo CONFIG && cat collector/shared-env"
}

