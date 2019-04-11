GLOBAL_SOURCE_ROOT=$(pwd)/..

runGCPCosTestViaSSH() {
  local GCP_VM_NAME="$1"
  shift
  export DOCKER_USER="$1"
  shift
  export DOCKER_PASS="$1"
  shift
  local SOURCE_ROOT="$1"
  shift
  createGCPVMCos "$GCP_VM_NAME" "$SOURCE_ROOT"
  sleep 30
  if gcloud compute ssh "$GCP_VM_NAME" --command "test -d collector" ; then
    echo "(collector/ already copied it seems)"
  else
    buildSourceTarballWorkingDir "$SOURCE_ROOT"
    scpSourceTarballToGcpHost "$GCP_VM_NAME"
    extractSourceTarballViaGCPSSH "$GCP_VM_NAME"
  fi
  gcloud compute ssh "$GCP_VM_NAME" --command "sudo toolbox uname -a"
  if gcloud compute ssh "$GCP_VM_NAME" --command 'pwd && docker version && ls -l /var/run/docker.sock && date && echo TOOLBOX && sudo toolbox --bind /var/run:/extrun --overlay $(pwd):$(pwd) --overlay /tmp:/tmp $(pwd)/collector/.circleci/coshelp/oncos'" $DOCKER_USER $DOCKER_PASS" -- -t ; then
    echo "Command succeeded"
    return 0
  else
    echo "Command failed"
    return 1
  fi
}

