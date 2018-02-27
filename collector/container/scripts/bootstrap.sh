#!/bin/bash

function test {
    "$@"
    local status=$?
    if [ $status -ne 0 ]; then
        echo "Error with $1" >&2
        exit $status
    fi
    return $status
}

function remove_collector_module() {
    if lsmod | grep -q collector; then
        echo "Collector kernel module has already been loaded."
        echo "Removing so that collector can insert it at startup."
        test rmmod collector
    fi
}

function download_kernel_module() {
    MODULE_NAME="$1"
    KERNEL_MODULE=$MODULE_NAME-$KERNELVERSION.ko
    echo >&2
    echo "Attempting to download $KERNEL_MODULE." >&2
    wget -O $MODULE_NAME.ko $MODULE_URL/$DISTRO/$KERNEL_MODULE
    if [ $? -ne 0 ]; then
      echo "Error downloading $DISTRO/$KERNEL_MODULE from remote repository." >&2
      return 1
    fi
    mkdir -p /module/
    cp $MODULE_NAME.ko /module/$MODULE_NAME.ko
    chmod 777 /module/$MODULE_NAME.ko
    echo "Using $DISTRO/$KERNEL_MODULE downloaded from remote repository." >&2
    return 0
}

function find_kernel_module() {
    MODULE_NAME="$1"
    KERNEL_MODULE=$MODULE_NAME-$KERNELVERSION.ko
    echo >&2
    echo "Attempting to find built-in $KERNEL_MODULE." >&2
    EXPECTED_PATH="/kernel-modules/$DISTRO/$KERNEL_MODULE"
    if [ -f "$EXPECTED_PATH" ]; then
      mkdir -p /module/
      cp "$EXPECTED_PATH" /module/$MODULE_NAME.ko
      chmod 777 /module/$MODULE_NAME.ko
      echo "Using built-in $EXPECTED_PATH." >&2
      return 0
    fi
    echo "Didn't find $KERNEL_MODULE built-in." >&2
    return 1
}

# Get the hostname from Docker so this container can use it in its output.
HOSTNAME=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .Name)
echo "Setting this container's node name to $HOSTNAME."
mkdir -p /host/etc/
echo "$HOSTNAME" > /host/etc/hostname

# kernel module download/build
KERNELVERSION=`uname -r`
if [ -z ${KERNEL_VERSION+x} ]; then
    echo "KERNELVERSION set to $KERNELVERSION.";
else
    KERNELVERSION=$KERNEL_VERSION
    echo "KERNELVERSION is set to '$KERNEL_VERSION'.";
fi

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

find_kernel_module collector
if [ $? -ne 0 ]; then
  download_kernel_module collector
  if [ $? -ne 0 ]; then
    echo "Could not find or download a collector module."
    exit 1
  fi
fi

# The collector binary will insert the collector module.
remove_collector_module

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
echo "Exec $@"
# Signal handler for SIGTERM
trap 'clean_up' TERM QUIT INT
eval "exec $@" &
PID=$!
wait $PID
remove_collector_module
