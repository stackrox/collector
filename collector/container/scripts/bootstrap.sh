#!/bin/bash

if [[ "$ROX_PLATFORM" == "swarm" ]]; then
    ## swap to reentrant container strategy to gain privileged mode
    exec /swarm_entrypoint.sh "$@"
fi

MODULE_NAME="collector"
MODULE_PATH="/module/${MODULE_NAME}.ko"

KERNEL_VERSION=$(uname -r)
echo "Kernel version detected: $KERNEL_VERSION."

MODULE_VERSION="$(cat /kernel-modules/MODULE_VERSION.txt)"
if [[ -n "$MODULE_VERSION" ]]; then
    echo "Module version detected: $MODULE_VERSION"
    if [[ -n "$MODULE_DOWNLOAD_BASE_URL" ]]; then
        MODULE_URL="${MODULE_DOWNLOAD_BASE_URL}/${MODULE_VERSION}"
    fi
fi

KERNEL_MODULE="${MODULE_NAME}-${KERNEL_VERSION}.ko"

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

function download_kernel_module() {
    if [[ -z "$MODULE_URL" ]]; then
        echo "Downloading modules not supported."
        return 1
    fi
    local URL="$MODULE_URL/$KERNEL_MODULE"
    if curl -L -s -o "${MODULE_PATH}.gz" "${URL}.gz"; then
        gunzip "${MODULE_PATH}.gz"
    elif ! curl -L -s -o "$MODULE_PATH" "$URL"; then
      echo "Error downloading $MODULE_NAME module for kernel version $KERNEL_VERSION." >&2
      return 1
    fi
    echo "Using downloaded $MODULE_NAME module for kernel version $KERNEL_MODULE." >&2
    return 0
}

function find_kernel_module() {
    EXPECTED_PATH="/kernel-modules/$KERNEL_MODULE"
    if [[ -f "${EXPECTED_PATH}.gz" ]]; then
      gunzip -c "${EXPECTED_PATH}.gz" >"$MODULE_PATH"
    elif [ -f "$EXPECTED_PATH" ]; then
      cp "$EXPECTED_PATH" "$MODULE_PATH"
    else
      echo "Didn't find $KERNEL_MODULE built-in." >&2
      return 1
    fi
    echo "Using built-in $MODULE_NAME module for kernel version $KERNEL_VERSION." >&2
    return 0
}


# Get the hostname from Docker so this container can use it in its output.
export NODE_HOSTNAME=""
NODE_HOSTNAME=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .Name)


mkdir -p /module/
if ! find_kernel_module ; then
  if ! download_kernel_module || [[ ! -f "$MODULE_PATH" ]]; then
    echo "The $MODULE_NAME module may not have been compiled for this version yet." >&2
    echo "Please provide this complete error message to StackRox support." >&2
    echo "This program will now exit and retry when it is next restarted." >&2
    echo "" >&2
    exit 1
  fi
fi

chmod 0444 "$MODULE_PATH"

# The collector program will insert the kernel module upon startup.
remove_module

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
