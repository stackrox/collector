GLOBAL_SOURCE_ROOT=$(pwd)/..

runGCPCosTestViaSSH() {
  local GCP_VM_NAME="$1"
  shift
  local SOURCE_ROOT="$GLOBAL_SOURCE_ROOT"
  shift
  createGCPVMCos "$GCP_VM_NAME" "$GSOURCE_ROOT"
  sleep 30
#  local GDOCKER_USER="$1"
#  shift
#  local GDOCKER_PASS="$1"
#  shift
#  local GSOURCE_ROOT="$1"
#  shift
  if gcloud compute ssh "$GCP_VM_NAME" --command "test -d collector" ; then
    echo "(collector/ already copied it seems)"
  else
    buildSourceTarballWorkingDir "$SOURCE_ROOT"
    scpSourceTarballToGcpHost "$GCP_VM_NAME"
    extractSourceTarballViaGCPSSH "$GCP_VM_NAME"
  fi
  gcloud compute ssh "$GCP_VM_NAME" --command "sudo toolbox uname -a"
  if gcloud compute ssh "$GCP_VM_NAME" --command 'pwd && docker version && ls -l /var/run/docker.sock && date && echo TOOLBOX && sudo toolbox --bind /var/run:/extrun --overlay $(pwd):$(pwd) $(pwd)/collector/.circleci/coshelp/oncos'" $DOCKER_USER $DOCKER_PASS" -- -t ; then
    echo "Command succeeded"
    return 0
  else
    echo "Command failed"
    return 1
  fi
}

