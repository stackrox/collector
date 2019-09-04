setupGCPVM() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_VM_TYPE="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift

  local GCP_VM_USER="$(whoami)"

  if test "$GCP_VM_TYPE" = "ubuntu" ; then
    createGCPVMUbuntu "$GCP_VM_NAME"
  elif test "$GCP_VM_TYPE" = "rhel" ; then
    createGCPVMRHEL "$GCP_VM_NAME"
  elif test "$GCP_VM_TYPE" = "cos" ; then
    createGCPVMCOS "$GCP_VM_NAME"
  elif test "$GCP_VM_TYPE" = "coreos" ; then
    createGCPVMCoreOS "$GCP_VM_NAME"
    GCP_VM_USER="core"
  else
    echo "Invalid GPC_VM_TYPE: $GCP_VM_TYPE"
    exit 1
  fi
  
  if ! gcpSSHReady "$GCP_VM_USER" "$GCP_VM_NAME" "$GCP_SSH_KEY_FILE"; then
    echo "GCP SSH failure"
    exit 1
  fi

  if test "$GCP_VM_TYPE" = "ubuntu" ; then
    installDockerOnUbuntuViaGCPSSH "$GCP_VM_NAME" "$GCP_SSH_KEY_FILE"
  elif test "$GCP_VM_TYPE" = "rhel" ; then
    installDockerOnRHELViaGCPSSH "$GCP_VM_NAME" "$GCP_SSH_KEY_FILE"
  fi

  loginDockerViaGCPSSH "$GCP_VM_USER" "$GCP_VM_NAME" "$GCP_SSH_KEY_FILE" "$GDOCKER_USER" "$GDOCKER_PASS" 
}
