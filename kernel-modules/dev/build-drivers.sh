#!/bin/bash
set -euo pipefail

OUTPUT_DIR="/collector/kernel-modules/container/kernel-modules"

source /collector/kernel-modules/build/utils.sh

package_probe() {
    kernel=$1
    probe_object=$2

    gzip -c "${probe_object}" \
        > "${OUTPUT_DIR}/collector-ebpf-${kernel}.o.gz"
}

KERNEL_VERSION="$(uname -r)"
FALCO_DIR="/collector/falcosecurity-libs"
PROBE_DIR="/collector/kernel-modules/probe"

setup_environment "${FALCO_DIR}/driver" "/collector/"
setup_driver_config "${FALCO_DIR}"

make -C ${PROBE_DIR} FALCO_DIR="${FALCO_DIR}/driver/bpf"

mkdir -p "${OUTPUT_DIR}"
package_probe "$KERNEL_VERSION" "$PROBE_DIR/probe.o"
