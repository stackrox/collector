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

function remove_sysdig_module() {
    if lsmod | grep -q sysdig_probe; then
        echo "sysdig-probe has already been loaded. Removing and relegating the task to the collector..."
        test rmmod sysdig-probe.ko
    fi
}

function download_kernel_module() {
    KERNEL_MODULE=$KERNELVERSION-$DISTRO-probe.ko
    wget -O sysdig-probe.ko https://storage.googleapis.com/stackrox-collector/$KERNEL_MODULE
    if [ $? -ne 0 ]; then
      echo "Error downloading $KERNEL_MODULE from remote stackrox repository." >&2
      exit $status
    fi
    mkdir -p /driver/
    cp sysdig-probe.ko /driver/sysdig-probe.ko
  	chmod 777 /driver/sysdig-probe.ko
}

# Get the hostname from Docker so this container can use it in its output.
HOSTNAME=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .Name)
echo "Setting this container's hostname to $HOSTNAME"
mkdir -p /host/etc/
echo "$HOSTNAME" > /host/etc/hostname

#sysdig start
KERNELVERSION=`uname -r`
OS_DETAILS=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .OperatingSystem)
if echo $OS_DETAILS | grep -qi Ubuntu; then
	DISTRO="ubuntu"
elif echo $OS_DETAILS | grep -qi centos; then
	DISTRO="centos"
elif echo $OS_DETAILS | grep -qi "Red Hat"; then
	DISTRO="rhel"
else
	echo "Distribution $OS_DETAILS not supported by stackrox. Please contact Stackrox for support."
	exit 1
fi

download_kernel_module
remove_sysdig_module
#sysdig done

echo "COLLECTOR_CONFIG = $COLLECTOR_CONFIG"
echo "CHISEL = $CHISEL"

echo "Exec $@"
exec "$@"
