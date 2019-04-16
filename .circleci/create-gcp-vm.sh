main() {
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift

  local BASHMODROOT="$GSOURCE_ROOT/.circleci" # will change later
  pushd "$BASHMODROOT"
  source moba.sh
  popd

  runCircleGCPUbuntuTestViaSSH "$GDOCKER_USER" "$GDOCKER_PASS" "$GSOURCE_ROOT"

  echo "A008"
  return 0
}

makeRegisterFilenameFromSymbol() {
  local GCP_SYMBOL_VAL
  GCP_SYMBOL_VAL=$1
  if [[ -z "$CI" ]] ; then
    echo -n "/tmp/collector-gcpnode-$GCP_SYMBOL_VAL-status.reg"
  else
    echo -n "$HOME/workspace/collector-gcpnode-$GCP_SYMBOL_VAL-status.reg"
  fi
  return 0
}

waitForDeleteInSlots() {
  local GCP_SYMBOL_VAL
  sleep 5
  for GCP_SYMBOL_VAL in $* ; do
    local REGISTERFILENAME=$(makeRegisterFilenameFromSymbol $GCP_SYMBOL_VAL)
    if test -f "$REGISTERFILENAME" ; then
      source $REGISTERFILENAME
      wait $GCLOUD_DELETE_PID || true
      echo "Done waiting for $GCP_SYMBOL_VAL , verifying deletion..."
      if confirmGCPVMSSHWorks $VM_NAME ; then
        echo "ERROR in deletion of VM $GCP_SYMBOL_VAL ."
        return 1
      else
        rm "$REGISTERFILENAME" || true
        echo "Deletion verified successful."
      fi
    else
      echo "WARNING: Register file missing for slot $GCP_SYMBOL_VAL skipping"
    fi
  done
  return 0
}

waitForCreateInSlots() {
  local GCP_SYMBOL_VAL
  for GCP_SYMBOL_VAL in $* ; do
    local REGISTERFILENAME=$(makeRegisterFilenameFromSymbol $GCP_SYMBOL_VAL)
    if test -f "$REGISTERFILENAME" ; then
      source $REGISTERFILENAME
      wait $GCLOUD_CREATE_PID || true
      echo "Done waiting for $GCP_SYMBOL_VAL , verifying creation..."
      if checkGCPVMExists $VM_NAME $ZONE ; then
        echo "Creation verified successful."
      else
        echo "ERROR in creation of VM $GCP_SYMBOL_VAL ."
        return 1
      fi
    else
      echo "WARNING: Register file missing for slot $GCP_SYMBOL_VAL skipping"
    fi
  done
  return 0
}

confirmGCPVMSSHWorks() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_ZONE="$1"
  shift
  if gcloud compute ssh \
    --zone "$GCP_ZONE" \
      "$GCP_VM_NAME" --command true ; then
    return 0
  else
    return 1
  fi
}

checkGCPVMExists() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_ZONE="$1"
  shift
  if gcloud compute instances describe \
    --zone "$GCP_ZONE" \
      "$GCP_VM_NAME" ; then
    return 0
  else
    return 1
  fi
}

deleteGCPVM() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_SYMBOL_VAL="$1"
  shift
  [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
  [ -z "$GCP_SYMBOL_VAL" ] && echo "error: missing parameter GCP_SYMBOL_VAL" && return 1

  local REGISTERFILENAME=$(makeRegisterFilenameFromSymbol $GCP_SYMBOL_VAL)
  if test -f "$REGISTERFILENAME" ; then
    source "$REGISTERFILENAME"
    ZONEOPT="--zone $ZONE"
  fi
  gcloud compute instances delete --quiet $ZONEOPT "$GCP_VM_NAME" &
  GCLOUD_DELETE_PID=$!
  echo "GCLOUD_DELETE_PID=$GCLOUD_DELETE_PID" >> $REGISTERFILENAME
  echo "VM_NAME=$GCP_VM_NAME" >> $REGISTERFILENAME
  echo "wait $GCLOUD_DELETE_PID || true"
  return 0
}
  

createGCPVM() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_SYMBOL_VAL="$1"
  shift
  local GCP_IMAGE_FAMILY="$1"
  shift
  local GCP_IMAGE_PROJECT="$1"
  shift
  [ -z "$GCP_VM_NAME" ] && echo "error: missing parameter GCP_VM_NAME" && return 1
  [ -z "$GCP_SYMBOL_VAL" ] && echo "error: missing parameter GCP_SYMBOL_VAL" && return 1
  [ -z "$GCP_IMAGE_FAMILY" ] && echo "error: missing parameter GCP_IMAGE_FAMILY" && return 1
  [ -z "$GCP_IMAGE_PROJECT" ] && echo "error: missing parameter GCP_IMAGE_PROJECT" && return 1
  local REGION=us-central1
  local ZONE=us-central1-a

  local REGISTERFILENAME=$(makeRegisterFilenameFromSymbol $GCP_SYMBOL_VAL)

  if [[ -z "$CI" ]] ; then
    SERVICE_ACCT=""
  else
    SERVICE_ACCT="--service-account=circleci-collector@stackrox-ci.iam.gserviceaccount.com"
  fi
  gcloud compute instances create \
    $SERVICE_ACCT \
    --image-family "$GCP_IMAGE_FAMILY" \
    --image-project "$GCP_IMAGE_PROJECT" \
    --boot-disk-size=20GB \
    --zone "$ZONE" \
      "$GCP_VM_NAME" &
  GCLOUD_CREATE_PID=$!
  echo "GCLOUD_CREATE_PID=$GCLOUD_CREATE_PID" > $REGISTERFILENAME
  echo "VM_NAME=$GCP_VM_NAME" >> $REGISTERFILENAME
  echo "ZONE=$ZONE" >> $REGISTERFILENAME
  echo "wait $GCLOUD_CREATE_PID || true"
  return 0
}
