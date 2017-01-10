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

function install_kernel_headers_ubuntu() {
    echo "deb http://security.ubuntu.com/ubuntu trusty-security main" > /etc/apt/sources.list
    echo "deb http://ftp.us.debian.org/debian jessie main" >> /etc/apt/sources.list

    apt-get update && apt-get install -y linux-headers-$KERNELVERSION

    test make KERNELRELEASE=$KERNELVERSION -C /lib/modules/$KERNELVERSION/build M=/driver clean modules
 }

function install_kernel_headers_centos() {
	yum install -y kernel-devel-$(uname -r)
    test make KERNELRELEASE=$KERNELVERSION -C /usr/src/kernels/$KERNELVERSION/ M=/driver clean modules
}

# Get the hostname from Docker so this container can use it in its output.
HOSTNAME=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .Name)
echo "Setting this container's hostname to $HOSTNAME"
mkdir -p /host/etc/
echo "$HOSTNAME" > /host/etc/hostname

#sysdig start
KERNELVERSION=`uname -r`
OS_DETAILS=$(uname -a)
if echo $OS_DETAILS | grep -qi ubuntu; then
    install_kernel_headers_ubuntu
    remove_sysdig_module
elif echo $OS_DETAILS | grep -qi debian; then
    install_kernel_headers_ubuntu
    remove_sysdig_module
elif echo $OS_DETAILS | grep -qi centos; then
    install_kernel_headers_centos
    remove_sysdig_module
elif echo $OS_DETAILS | grep -qi rhel; then
    install_kernel_headers_centos
    remove_sysdig_module
else
	remove_sysdig_module
fi
#sysdig done

# Uncomment this to enable generation of core for Collector
# echo '/core/core.%e.%p.%t' | sudo tee /proc/sys/kernel/core_pattern

echo "COLLECTOR_CONFIG = $COLLECTOR_CONFIG"
echo "CHISEL = $CHISEL"

echo "Exec $@"
exec "$@"
