#!/bin/bash
set -euo pipefail

OUTPUT_DIR="/collector/kernel-modules/container/kernel-modules"

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
