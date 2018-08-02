#!/bin/bash
MODULE_NAME="collector"

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
    KERNEL_MODULE=$MODULE_NAME-$KERNEL_VERSION.ko
    if [[ ! -d /module ]]; then
      echo >&2 "/module directory does not exist."
      return 1
    fi
    local URL="$MODULE_URL/$DISTRO/$KERNEL_MODULE"
    wget --no-verbose -O /module/$MODULE_NAME.ko "$URL"
    if [ $? -ne 0 ]; then
      echo "Error downloading $MODULE_NAME module for $DISTRO kernel version $KERNEL_VERSION." >&2
      return 1
    fi
    chmod 0444 /module/$MODULE_NAME.ko
    echo "Using downloaded $MODULE_NAME module for $DISTRO kernel version $KERNEL_MODULE." >&2
    return 0
}

function find_kernel_module() {
    KERNEL_MODULE=$MODULE_NAME-$KERNEL_VERSION.ko
    EXPECTED_PATH="/kernel-modules/$DISTRO/$KERNEL_MODULE"
    if [ -f "$EXPECTED_PATH" ]; then
      mkdir -p /module/
      cp "$EXPECTED_PATH" /module/$MODULE_NAME.ko
      chmod 777 /module/$MODULE_NAME.ko
      echo "Using built-in $MODULE_NAME module for $DISTRO kernel version $KERNEL_VERSION." >&2
      return 0
    fi
    echo "Didn't find $KERNEL_MODULE built-in." >&2
    return 1
}

if [ "$ROX_PLATFORM" = "swarm" ]; then
    ## swap to reentrant container strategy to gain privileged mode
    exec /swarm_entrypoint.sh "$@"
fi

# Get the hostname from Docker so this container can use it in its output.
export NODE_HOSTNAME=""
NODE_HOSTNAME=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .Name)

KERNEL_VERSION=`uname -r`
echo "Kernel version detected: $KERNEL_VERSION.";

OS_DETAILS=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .OperatingSystem)

if echo $OS_DETAILS | grep -qi Ubuntu; then
    DISTRO="Ubuntu"
elif echo $OS_DETAILS | grep -qi debian; then
    DISTRO="Debian"
elif echo $OS_DETAILS | grep -qi centos; then
    DISTRO="RedHat"
elif echo $OS_DETAILS | grep -qi "Red Hat"; then
    DISTRO="RedHat"
elif echo $OS_DETAILS | grep -qi OpenShift; then
    DISTRO="RedHat"
elif echo $OS_DETAILS | grep -qi coreos; then
    DISTRO="CoreOS"
else
    echo "Distribution '$OS_DETAILS' is not supported."
    exit 1
fi

find_kernel_module
if [ $? -ne 0 ]; then
  download_kernel_module
  if [ $? -ne 0 ]; then
    echo "The $MODULE_NAME module may not have been compiled for this version yet." >&2
    echo "Please provide this complete error message to StackRox support." >&2
    echo "This program will now exit and retry when it is next restarted." >&2
    echo "" >&2
    exit 1
  fi
fi

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
