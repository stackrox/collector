#!/bin/bash
set -euo pipefail

OUTPUT_DIR="/collector/kernel-modules/container/kernel-modules"

package_kmod() {
    kernel=$1
    driver_dir=$2

    gzip -c "${driver_dir}/build/driver/collector.ko" \
        > "${OUTPUT_DIR}/collector-${kernel}.ko.gz"
}

package_probe() {
    kernel=$1
    driver_dir=$2

    gzip -c "${driver_dir}/build/driver/bpf/probe.o" \
        > "${OUTPUT_DIR}/collector-ebpf-${kernel}.o.gz"
}

KERNEL_VERSION="$(uname -r)"
DRIVER_DIR="/collector/falcosecurity-libs"

mkdir -p "${DRIVER_DIR}/build"

cmake -S ${DRIVER_DIR} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-fno-pie" \
    -DPROBE_NAME=collector \
    -DBUILD_USERSPACE=OFF \
    -DBUILD_DRIVER=ON \
    -DENABLE_DKMS=OFF \
    -DBUILD_BPF=ON \
    -B ${DRIVER_DIR}/build
make -C ${DRIVER_DIR}/build/driver

mkdir -p "${OUTPUT_DIR}"
package_kmod "$KERNEL_VERSION" "$DRIVER_DIR"
package_probe "$KERNEL_VERSION" "$DRIVER_DIR"

# No reason to leave this hanging about
rm -rf "${DRIVER_DIR}/build"
