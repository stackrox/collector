#!/bin/bash

MODULE_NAME="collector"
MODULE_PATH="/module/${MODULE_NAME}.ko"
PROBE_NAME="collector-ebpf"
PROBE_PATH="/module/${PROBE_NAME}.o"

KERNEL_VERSION=$(uname -r)
echo "Kernel version detected: $KERNEL_VERSION."

export KERNEL_MAJOR=""
export KERNEL_MINOR=""
KERNEL_MAJOR=$(echo ${KERNEL_VERSION} | cut -d. -f1)
KERNEL_MINOR=$(echo ${KERNEL_VERSION} | cut -d. -f2)

MODULE_VERSION="$(cat /kernel-modules/MODULE_VERSION.txt)"
if [[ -n "$MODULE_VERSION" ]]; then
    echo "Module version detected: $MODULE_VERSION"
    if [[ -n "$MODULE_DOWNLOAD_BASE_URL" ]]; then
        MODULE_URL="${MODULE_DOWNLOAD_BASE_URL}/${MODULE_VERSION}"
    fi
fi

if [ -f "/host/etc/os-release" ]; then
    # Source the contents of /etc/os-release to determine if on COS
    . "/host/etc/os-release"
    
    if [ ! -z ${ID+x} ] && [ "${ID}" == "cos" ]; then
        # check that last char of KERNEL_VERSION is '+' and BUILD_ID is defined/non-empty.
        if [ "${KERNEL_VERSION: -1}" = "+" ] && [ ! -z ${BUILD_ID+x} ]; then
            KERNEL_VERSION="$(echo ${KERNEL_VERSION} | sed 's/.$//')-${BUILD_ID}-${ID}"
        fi 
    fi
fi

KERNEL_MODULE="${MODULE_NAME}-${KERNEL_VERSION}.ko"
KERNEL_PROBE="${PROBE_NAME}-${KERNEL_VERSION}.o"

function test {
    "$@"
    local status=$?
    if [ $status -ne 0 ]; then
        echo "Error with $1" >&2
        exit $status
    fi
    return $status
}

function remove_module() {
    if lsmod | grep -q "$MODULE_NAME"; then
        echo "Collector kernel module has already been loaded."
        echo "Removing so that collector can insert it at startup."
        test rmmod "$MODULE_NAME"
    fi
}

function download_kernel_object() {
    local KERNEL_OBJECT="$1"
    local OBJECT_PATH="$2"
    if [[ -z "$MODULE_URL" ]]; then
        echo "Downloading modules not supported."
        return 1
    fi
    local URL="$MODULE_URL/$KERNEL_OBJECT"
    local FILENAME_GZ="$OBJECT_PATH.gz"
    if ! curl -w "%{http_code}" -L -s -o "$FILENAME_GZ" "${URL}.gz" >/tmp/curlret.log 2>/tmp/curlret.err ; then
        echo "Error downloading $KERNEL_OBJECT for kernel version $KERNEL_VERSION. curl exit code $?" >&2
        cat /tmp/curlret.err >&2
        rm /tmp/curlret.err /tmp/curlret.log
        return 1
    fi
    if test $(cat /tmp/curlret.log) != "200" ; then
        echo "Error downloading $KERNEL_OBJECT for kernel version $KERNEL_VERSION. http status code $(cat /tmp/curlret.log)" >&2
        rm /tmp/curlret.err /tmp/curlret.log
        return 1
    fi
    rm /tmp/curlret.err /tmp/curlret.log
    gunzip "$FILENAME_GZ"
    echo "Using downloaded $KERNEL_OBJECT for kernel version $KERNEL_VERSION." >&2
    return 0
}

function find_kernel_object() {
    local KERNEL_OBJECT="$1"
    local OBJECT_PATH="$2"
    EXPECTED_PATH="/kernel-modules/$KERNEL_OBJECT"
    if [[ -f "${EXPECTED_PATH}.gz" ]]; then
      gunzip -c "${EXPECTED_PATH}.gz" >"$OBJECT_PATH"
    elif [ -f "$EXPECTED_PATH" ]; then
      cp "$EXPECTED_PATH" "$OBJECT_PATH"
    else
      echo "Didn't find $KERNEL_OBJECT built-in." >&2
      return 1
    fi
    echo "Using built-in $KERNEL_OBJECT for kernel version $KERNEL_VERSION." >&2
    return 0
}

function kernel_supports_ebpf() {
    # Kernel version >= 4.14
    if [[ $KERNEL_MAJOR -lt 4 || ( $KERNEL_MAJOR -eq 4 && $KERNEL_MINOR -lt 14 ) ]]; then
        return 1
    fi
    return 0
}

# Get the hostname from Docker so this container can use it in its output.
export NODE_HOSTNAME=""
NODE_HOSTNAME=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .Name)


mkdir -p /module/
if ! find_kernel_object "${KERNEL_MODULE}" "${MODULE_PATH}"; then
  if ! download_kernel_object "${KERNEL_MODULE}" "${MODULE_PATH}" || [[ ! -f "$MODULE_PATH" ]]; then
    echo "The $MODULE_NAME module may not have been compiled for this version yet." >&2
  fi
fi

chmod 0444 "$MODULE_PATH"

# The collector program will insert the kernel module upon startup.
remove_module

if kernel_supports_ebpf; then
  if ! find_kernel_object "${KERNEL_PROBE}" "${PROBE_PATH}"; then
    if ! download_kernel_object "${KERNEL_PROBE}" "${PROBE_PATH}" || [[ ! -f "$PROBE_PATH" ]]; then
      echo "The $PROBE_NAME probe may not have been compiled for this version yet." >&2
    fi
  fi
  if [[ -f "$PROBE_PATH" ]]; then
    chmod 0444 "$PROBE_PATH"
  fi
fi

if [[ ! -f "$PROBE_PATH" ]] && [[ ! -f "$MODULE_PATH" ]]; then
    echo "No module or probe found for kernel version $KERNEL_VERSION." >&2
    echo "Please provide this complete error message to StackRox support." >&2
    echo "This program will now exit and retry when it is next restarted." >&2
    echo "" >&2
    exit 1
fi

# Uncomment this to enable generation of core for Collector
# echo '/core/core.%e.%p.%t' > /proc/sys/kernel/core_pattern

echo "COLLECTOR_CONFIG = $COLLECTOR_CONFIG"
echo "CHISEL = $CHISEL"

clean_up() {
    echo "collector pid to be stopped is $PID"
    kill -TERM $PID; wait $PID
}

# Remove "/bin/sh -c" from arguments
shift;shift
echo "Exec $*"
# Signal handler for SIGTERM
trap 'clean_up' TERM QUIT INT
eval exec "$@" &
PID=$!
wait $PID
remove_module
