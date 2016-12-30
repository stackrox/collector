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

function build_kernel_module_ubuntu() {
    echo "Attempting to build the kernel module for $KERNELVERSION-$DISTRO"

    echo "deb http://security.ubuntu.com/ubuntu trusty-security main" > /etc/apt/sources.list
    echo "deb http://ftp.us.debian.org/debian jessie main" >> /etc/apt/sources.list

    apt-get update && apt-get install -y linux-headers-$KERNELVERSION

    test make KERNELRELEASE=$KERNELVERSION -C /lib/modules/$KERNELVERSION/build M=/driver clean modules
}

function build_kernel_module_centos() {
    echo "Attempting to build the kernel module for $KERNELVERSION-$DISTRO"

    yum install -y kernel-devel-$KERNELVERSION
    test make KERNELRELEASE=$KERNELVERSION -C /usr/src/kernels/$KERNELVERSION/ M=/driver clean modules
}

function build_kernel_module_rhel() {
    echo "Attempting to build the kernel module for $KERNELVERSION-$DISTRO"

    yum install -y kernel-devel-$KERNELVERSION
    test make KERNELRELEASE=$KERNELVERSION -C /usr/src/kernels/$KERNELVERSION/ M=/driver clean modules
}

function download_kernel_module() {
    KERNEL_MODULE=$KERNELVERSION-$DISTRO-probe.ko
    wget -O sysdig-probe.ko https://$MODULE_URL/$KERNEL_MODULE
    if [ $? -ne 0 ]; then
      echo "Error downloading $KERNEL_MODULE from remote stackrox repository." >&2
      return 1
    fi
    mkdir -p /driver/
    cp sysdig-probe.ko /driver/sysdig-probe.ko
    chmod 777 /driver/sysdig-probe.ko
    return 0
}

# Get the hostname from Docker so this container can use it in its output.
HOSTNAME=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .Name)
echo "Setting this container's hostname to $HOSTNAME"
mkdir -p /host/etc/
echo "$HOSTNAME" > /host/etc/hostname

# kernel module download/build
KERNELVERSION=`uname -r`
if [ -z ${KERNEL_VERSION+x} ];
    then echo "KERNELVERSION set to $KERNELVERSION";
else
    KERNELVERSION=$KERNEL_VERSION
    echo "KERNELVERSION is set to '$KERNEL_VERSION'";
fi

OS_DETAILS=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .OperatingSystem)

if echo $OS_DETAILS | grep -qi Ubuntu; then
    DISTRO="ubuntu"
    download_kernel_module
    if [ $? -ne 0 ]; then
        build_kernel_module_ubuntu
    fi
    remove_sysdig_module
elif echo $OS_DETAILS | grep -qi debian; then
    DISTRO="ubuntu"
    download_kernel_module
    if [ $? -ne 0 ]; then
        build_kernel_module_ubuntu
    fi
    remove_sysdig_module
elif echo $OS_DETAILS | grep -qi centos; then
    DISTRO="centos"
    download_kernel_module
    if [ $? -ne 0 ]; then
        build_kernel_module_centos
    fi
    remove_sysdig_module
elif echo $OS_DETAILS | grep -qi "Red Hat"; then
    DISTRO="redhat"
    download_kernel_module
    if [ $? -ne 0 ]; then
        build_kernel_module_rhel
    fi
    remove_sysdig_module
else
    echo "Distribution $OS_DETAILS not supported by stackrox. Please contact Stackrox for support."
    exit 1
fi

# kernel module download/build

# Uncomment this to enable generation of core for Collector
# echo '/core/core.%e.%p.%t' | sudo tee /proc/sys/kernel/core_pattern

echo "COLLECTOR_CONFIG = $COLLECTOR_CONFIG"
echo "CHISEL = $CHISEL"

echo "Exec $@"
exec "$@"
