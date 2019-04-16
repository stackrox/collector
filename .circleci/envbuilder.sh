runGCPUbuntuTestViaSSH() {
  local GCP_VM_NAME="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift
  createGCPVMUbuntu "$GCP_VM_NAME" "$GSOURCE_ROOT"
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
  installVariousAptDepsViaGCPSSH "$GCP_VM_NAME"
  loginDockerViaGCPSSH "$GCP_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS"
  extractSourceTarballViaGCPSSH "$GCP_VM_NAME"
  cd ../../integration-tests
}

runCircleGCPUbuntuTestViaSSH() {
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift
  local CIRCLE_BUILD_VM_NAME="collector-nb-${CIRCLE_WORKFLOW_ID}"
  runGCPUbuntuTestViaSSH "$CIRCLE_BUILD_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS" "$GSOURCE_ROOT"
}

