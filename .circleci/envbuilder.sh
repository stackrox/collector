runGCPUbuntuTestViaSSH() {
  local GCP_VM_NAME="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift
  buildSourceTarballWorkingDir "$SOURCE_ROOT"
  scpSourceTarballToGcpHost "$GCP_VM_NAME"
  installVariousAptDepsViaGCPSSH "$GCP_VM_NAME"
  loginDockerViaGCPSSH "$GCP_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS"
  extractSourceTarballViaGCPSSH "$GCP_VM_NAME"
}

runGCPCosTestViaSSH() {
  local GCP_VM_NAME="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift
  if gcloud compute ssh "$GCP_VM_NAME" --command "test -d collector" ; then
    echo "(collector/ already copied it seems)"
  else
    buildSourceTarballWorkingDir "$SOURCE_ROOT"
    scpSourceTarballToGcpHost "$GCP_VM_NAME"
    extractSourceTarballViaGCPSSH "$GCP_VM_NAME"
  fi
  gcloud compute ssh "$GCP_VM_NAME" --command "sudo toolbox uname -a"
  if gcloud compute ssh "$GCP_VM_NAME" --command 'pwd && mkdir -m 777 /tmp/d && docker version && ls -l /var/run/docker.sock && date && echo TOOLBOX && ls -ld /tmp && ls -l /tmp && echo TMPLSDONE && sudo toolbox --bind /var/run:/extrun --overlay /tmp/d:/tmp --overlay $(pwd):$(pwd) $(pwd)/collector/.circleci/coshelp/oncos'" $DOCKER_USER $DOCKER_PASS" -- -t ; then
    echo "Command succeeded"
    return 0
  else
    echo "Command failed"
    return 1
  fi
}

runCircleGCPUbuntuTestViaSSH() {
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift
  local CIRCLE_BUILD_VM_NAME="collector-nb-${CIRCLE_WORKFLOW_ID}"
  runGCPUbuntuTestViaSSH "$CIRCLE_BUILD_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS" "$GSOURCE_ROOT"
}

