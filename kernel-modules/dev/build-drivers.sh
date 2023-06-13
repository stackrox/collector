#!/bin/bash
set -euo pipefail

OUTPUT_DIR="/collector/kernel-modules/container/kernel-modules"
BUILD_KERNEL_MODULE="${BUILD_KERNEL_MODULE:-false}"

package_kmod() {
    kernel=$1
    probe_object=$2

    gzip -c "${probe_object}" \
        > "${OUTPUT_DIR}/collector-${kernel}.ko.gz"
}

package_probe() {
    kernel=$1
    probe_object=$2

    gzip -c "${probe_object}" \
        > "${OUTPUT_DIR}/collector-ebpf-${kernel}.o.gz"
}

KERNEL_VERSION="$(uname -r)"
DRIVER_DIR="/collector/falcosecurity-libs"
PROBE_DIR="/collector/kernel-modules/probe"

mkdir -p "${OUTPUT_DIR}"

make -C ${PROBE_DIR} FALCO_DIR="${DRIVER_DIR}/driver/bpf"
package_probe "$KERNEL_VERSION" "$PROBE_DIR/probe.o"

if [[ "$BUILD_KERNEL_MODULE" != "false" ]]; then
    echo "Building kernel module"
    mkdir -p "${DRIVER_DIR}/build"

    cmake -S ${DRIVER_DIR} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="-fno-pie" \
        -DDRIVER_NAME=collector \
        -DDRIVER_DEVICE_NAME=collector \
        -DBUILD_USERSPACE=OFF \
        -DBUILD_DRIVER=ON \
        -DENABLE_DKMS=OFF \
        -DCREATE_TEST_TARGETS=OFF \
        -D__MODERN_BPF_DEBUG__=ON \
        -DBUILD_LIBSCAP_MODERN_BPF=ON \
        -DMODERN_BPF_EXCLUDE_PROGS='^(openat2|ppoll|setsockopt|getsockopt|clone3|io_uring_setup|nanosleep)$' \
        -B ${DRIVER_DIR}/build

    make -C ${DRIVER_DIR}/build/driver
    package_kmod "$KERNEL_VERSION" "$DRIVER_DIR/build/driver/collector.ko"

    # No reason to leave this hanging about
    rm -rf "${DRIVER_DIR}/build"
fi
