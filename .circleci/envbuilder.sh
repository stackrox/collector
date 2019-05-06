runGCPUbuntuTestViaSSH() {
  local GCP_VM_NAME="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift
  createGCPVMUbuntu "$GCP_VM_NAME"
  copySourceTarball"$GCP_VM_NAME" "$GSOURCE_ROOT"
  installVariousAptDepsViaGCPSSH "$GCP_VM_NAME"
  loginDockerViaGCPSSH "$GCP_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS"
  extractSourceTarballViaGCPSSH "$GCP_VM_NAME"
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
  local CIRCLE_BUILD_VM_NAME="collector-nb-${CIRCLE_BUILD_NUM}"
  runGCPUbuntuTestViaSSH "$CIRCLE_BUILD_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS" "$GSOURCE_ROOT"
}

setupGCPVM() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_VM_TYPE="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift

  if test "$GCP_VM_TYPE" = "ubuntu" ; then
    createGCPVMUbuntu "$GCP_VM_NAME"
    installDockerOnUbuntuViaGCPSSH "$GCP_VM_NAME"
  elif test "$GCP_VM_TYPE" = "rhel" ; then
    createGCPVMRHEL "$GCP_VM_NAME"
    installDockerOnRHELViaGCPSSH "$GCP_VM_NAME"
  elif test "$GCP_VM_TYPE" = "cos" ; then
    createGCPVMCOS "$GCP_VM_NAME"
  elif test "$GCP_VM_TYPE" = "coreos" ; then
    createGCPVMCoreOS "$GCP_VM_NAME"
  else
    echo "Invalid GPC_VM_TYPE: $GCP_VM_TYPE"
    exit 1
  fi
  loginDockerViaGCPSSH "$GCP_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS"
}
