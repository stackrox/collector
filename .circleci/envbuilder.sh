runGCPUbuntuTestViaSSH() {
  local GCP_VM_NAME="$1"
  createGCPVMUbuntu "$GCP_VM_NAME" "$GSOURCE_ROOT"
  installVariousAptDepsViaGCPSSH "$GCP_VM_NAME"
  loginDockerViaGCPSSH "$GCP_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS"
  extractSourceTarballViaGCPSSH "$GCP_VM_NAME"
}

runCircleGCPUbuntuTestViaSSH() {
  local CIRCLE_BUILD_VM_NAME="collector-nb-${CIRCLE_BUILD_NUM}"
  runGCPUbuntuTestViaSSH "$CIRCLE_BUILD_VM_NAME"
}

