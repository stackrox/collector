#!/bin/bash

log() { echo "$@" >&2; }

function get_os_release_value() {
    local key=$1
    local os_release="/host/etc/os-release"
    if [ ! -f "${os_release}" ]; then
        os_release="/host/usr/lib/os-release"
    fi
    if [ -f "${os_release}" ]; then
        while IFS="=" read var value; do
            if [ "$key" == "$var" ]; then
                echo $value
            fi
        done < "${os_release}"
    fi
}

function get_distro() {
    local distro=$(get_os_release_value "PRETTY_NAME")
    if [ -z "${distro}" ]; then
      echo "Linux"
    fi
    echo ${distro}
}

exit_with_error() {
    log ""
    log "Please provide this complete error message to StackRox support."
    log "This program will now exit and retry when it is next restarted."
    log ""
    exit 1
}

function test {
    "$@"
    local status=$?
    if [ $status -ne 0 ]; then
        log "Error with $1"
        exit $status
    fi
    return $status
}

function remove_module() {
    if lsmod | grep -q "$MODULE_NAME"; then
        log "Collector kernel module has already been loaded."
        log "Removing so that collector can insert it at startup."
        test rmmod "$MODULE_NAME"
    fi
}

function download_kernel_object() {
    local KERNEL_OBJECT="$1"
    local OBJECT_PATH="$2"
    local OBJECT_TYPE="kernel module"
    if [ "${KERNEL_OBJECT##*.}" == "o" ]; then
      OBJECT_TYPE="eBPF probe"
    fi

    if [[ -z "$MODULE_URL" ]]; then
        log "Collector is not configured to download the $OBJECT_TYPE"
        return 1
    fi
    local URL="$MODULE_URL/$KERNEL_OBJECT"
    local FILENAME_GZ="$OBJECT_PATH.gz"

    # Attempt to download kernel object
    local HTTP_CODE=$(curl -w "%{http_code}" -L -s -o "$FILENAME_GZ" "${URL}.gz" 2>/dev/null)

    if [ "$HTTP_CODE" != "200" ] ; then
        log "Error downloading $OBJECT_TYPE $KERNEL_OBJECT (Error code: $HTTP_CODE)"
        return 1
    fi

    gunzip "$FILENAME_GZ"
    log "Using downloaded $OBJECT_TYPE $KERNEL_OBJECT"
    return 0
}

function find_kernel_object() {
    local KERNEL_OBJECT="$1"
    local OBJECT_PATH="$2"
    local EXPECTED_PATH="/kernel-modules/$KERNEL_OBJECT"
    local OBJECT_TYPE="kernel module"
    if [ "${KERNEL_OBJECT##*.}" == "o" ]; then
      OBJECT_TYPE="eBPF probe"
    fi

    if [[ -f "${EXPECTED_PATH}.gz" ]]; then
      gunzip -c "${EXPECTED_PATH}.gz" >"${OBJECT_PATH}"
    elif [ -f "$EXPECTED_PATH" ]; then
      cp "$EXPECTED_PATH" "$OBJECT_PATH"
    else
      log "Didn't find $OBJECT_TYPE $KERNEL_OBJECT built-in."
      return 1
    fi

    log "Using built-in $OBJECT_TYPE $KERNEL_OBJECT"
    return 0
}

function kernel_supports_ebpf() {
    # Kernel version >= 4.14
    if [[ $KERNEL_MAJOR -lt 4 || ( $KERNEL_MAJOR -eq 4 && $KERNEL_MINOR -lt 14 ) ]]; then
        return 1
    fi
    return 0
}

function cos_host() {
    if [ "${ID}" == "cos" ]; then
        return 0
    fi
    return 1
}

function collection_method_module() {
    local collection_method="$(echo ${COLLECTION_METHOD} | tr '[:upper:]' '[:lower:]')"
    if [ "${collection_method}" == "kernel_module" ] || [ "${collection_method}" == "kernel-module"; then
        return 0
    fi
    return 1
}

function collection_method_ebpf() {
    local collection_method="$(echo ${COLLECTION_METHOD} | tr '[:upper:]' '[:lower:]')"
    if [ "${collection_method}" == "ebpf" ]; then
        return 0
    fi
    return 1
}

[ -n "${KERNEL_VERSION}" ] || { KERNEL_VERSION=$(uname -r); }

MODULE_NAME="collector"
MODULE_PATH="/module/${MODULE_NAME}.ko"
PROBE_NAME="collector-ebpf"
PROBE_PATH="/module/${PROBE_NAME}.o"

export KERNEL_MAJOR=""
export KERNEL_MINOR=""
KERNEL_MAJOR=$(echo ${KERNEL_VERSION} | cut -d. -f1)
KERNEL_MINOR=$(echo ${KERNEL_VERSION} | cut -d. -f2)

# Get and export the node hostname from Docker, needed by collector
export NODE_HOSTNAME=""
NODE_HOSTNAME=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .Name)

DISTRO="$(get_distro)"

# Needed for COS
BUILD_ID="$(get_os_release_value 'BUILD_ID')"
ID="$(get_os_release_value 'ID')"

log "Hostname: ${NODE_HOSTNAME}"
log "OS: ${DISTRO}"
log "Kernel Version: ${KERNEL_VERSION}"

MODULE_VERSION="$(cat /kernel-modules/MODULE_VERSION.txt)"
if [[ -n "$MODULE_VERSION" ]]; then
    log "Collector Version: $MODULE_VERSION"
    if [[ -n "$MODULE_DOWNLOAD_BASE_URL" ]]; then
        MODULE_URL="${MODULE_DOWNLOAD_BASE_URL}/${MODULE_VERSION}"
    fi
fi

if [ "${ID}" == "cos" ]; then
    # check that last char of KERNEL_VERSION is '+' and BUILD_ID is non-empty.
    if [ "${KERNEL_VERSION: -1}" = "+" ] && [ ! -z "${BUILD_ID}" ]; then
        # Use custom kernel_version for COS
        KERNEL_VERSION="$(echo ${KERNEL_VERSION} | sed 's/.$//')-${BUILD_ID}-${ID}"
    fi
fi
 
KERNEL_MODULE="${MODULE_NAME}-${KERNEL_VERSION}.ko"
KERNEL_PROBE="${PROBE_NAME}-${KERNEL_VERSION}.o"

mkdir -p /module

# Backwards compatability for releases older than 2.4.20
if [ -z "${COLLECTION_METHOD}" ]; then
  export COLLECTION_METHOD=""
  if [ "$(echo '${COLLECTOR_CONFIG}' | jq --raw-output .useEbpf)" == "true" ]; then
    COLLECTION_METHOD="EBPF"
  else
    COLLECTION_METHOD="KERNEL_MODULE"
  fi
fi

if ! collection_method_ebpf && ! collection_method_module ; then
  log "Error: Collector configured with invalid value: COLLECTION_METHOD=${COLLECTION_METHOD}"
  exit_with_error
fi

if ! collection_method_ebpf && collection_method_module && cos_host ; then
  log "Error: ${DISTRO} does not support third-party kernel modules"
  if kernel_supports_ebpf; then
    log "Warning: Switching to eBPF based collection, please configure RUNTIME_SUPPORT=ebpf"
    COLLECTION_METHOD="EBPF"
  else
    exit_with_error
  fi
fi

if collection_method_ebpf && ! kernel_supports_ebpf; then
  if cos_host; then 
    log "Error: ${DISTRO} does not support third-party kernel modules or the required eBPF features."
    exit_with_error
  else
    log "Error: ${DISTRO} ${KERNEL_VERSION} does not support ebpf based collection."
    log "Warning: Switching to kernel module based collection, please configure RUNTIME_SUPPORT=kernel-module"
    COLLECTION_METHOD="KERNEL_MODULE"
  fi
fi

if collection_method_module; then
  if ! find_kernel_object "${KERNEL_MODULE}" "${MODULE_PATH}"; then
    if ! download_kernel_object "${KERNEL_MODULE}" "${MODULE_PATH}" || [[ ! -f "$MODULE_PATH" ]]; then
      log "The kernel module may not have been compiled for version ${KERNEL_VERSION}."
    fi
  fi
  
  if [[ -f "$MODULE_PATH" ]]; then
    chmod 0444 "$MODULE_PATH"
  else
    log "Error: Failed to find kernel module for kernel version $KERNEL_VERSION."
    exit_with_error
  fi

  # The collector program will insert the kernel module upon startup.
  remove_module
fi

if collection_method_ebpf; then
  if kernel_supports_ebpf; then
    if ! find_kernel_object "${KERNEL_PROBE}" "${PROBE_PATH}"; then
      if ! download_kernel_object "${KERNEL_PROBE}" "${PROBE_PATH}" || [[ ! -f "$PROBE_PATH" ]]; then
        log "The ebpf probe may not have been compiled for version ${KERNEL_VERSION}."
      fi
    fi

    if [[ -f "$PROBE_PATH" ]]; then
      chmod 0444 "$PROBE_PATH"
    else
      log "Error: Failed to find ebpf probe for kernel version $KERNEL_VERSION."
      exit_with_error
    fi
  else
    log "Error: Kernel ${KERNEL_VERSION} doesn't support eBPF"
    exit_with_error
  fi
fi

if [[ ! -f "$PROBE_PATH" ]] && [[ ! -f "$MODULE_PATH" ]]; then
    exit_with_error
fi

# Uncomment this to enable generation of core for Collector
# echo '/core/core.%e.%p.%t' > /proc/sys/kernel/core_pattern

clean_up() {
    log "collector pid to be stopped is $PID"
    kill -TERM $PID; wait $PID
}

# Remove "/bin/sh -c" from arguments
shift;shift
log "Starting StackRox Collector..."  >&2
# Signal handler for SIGTERM
trap 'clean_up' TERM QUIT INT
eval exec "$@" &
PID=$!
wait $PID
remove_module
