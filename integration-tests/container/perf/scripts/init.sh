#!/usr/bin/env bash

# Based on script from https://github.com/mclenhard/ebpf-summit/blob/c5197e8e975979dfac6bd12094ea07cfd680db52/init/fetch-linux-headers.sh

set -exo pipefail

LSB_FILE="/etc/lsb-release"
OS_RELEASE_FILE="/etc/os-release"
TARGET_DIR="/usr/src"
HOST_MODULES_DIR="/lib/modules"

KERNEL_VERSION="${KERNEL_VERSION:-$(uname -r)}"

generate_headers() {
    echo "Generating kernel headers"

    cd "${BUILD_DIR}"
    if [ -e /proc/config.gz ]; then
        zcat /proc/config.gz > .config
    elif [ -e "/boot/config-${KERNEL_VERSION}" ]; then
        cp "/boot/config-${KERNEL_VERSION}" .config
    fi
    make ARCH=x86 olddefconfig > /dev/null
    make ARCH=x86 prepare > /dev/null

    # To ensure that all headers are copied, we can't use `make headers_install` or
    # `make headers_install_all`, which only copies a subset of headers, and doesn't
    # include some that are required by the various BPF tools.

    # To speed up the copy to the /usr/src directory, we can remove non-header files from
    # the source tree.
    find "${BUILD_DIR}" -regex '.*\.c\|.*\.txt\|.*Makefile\|.*Build\|.*Kconfig' -type f -delete
}

fetch_cos_linux_sources() {
    echo "Fetching upstream kernel sources."
    mkdir -p "${BUILD_DIR}"
    wget -O- "https://storage.googleapis.com/cos-tools/${BUILD_ID}/kernel-src.tar.gz" \
        | tar -xzf - -C "${BUILD_DIR}"
}

fetch_generic_linux_sources() {
    # 4.19.76-linuxkit -> 4.19.76
    # 4.14.154-128.181.amzn2.x86_64 -> 4.14.154
    # 4.19.76+gcp-something -> 4.19.76
    kernel_version="$(echo "${KERNEL_VERSION}" | awk -vFS='[-+]' '{ print $1 }')"
    major_version="$(echo "${KERNEL_VERSION}" | awk -vFS=. '{ print $1 }')"

    # Remove the '.0' as the intial kernel major release isn't published with a patch number.
    if [[ $kernel_version == *.0 ]]; then
        kernel_version="$(echo "${kernel_version}" | rev | sed s/0\.// | rev)"
    fi

    echo "Fetching upstream kernel sources for ${kernel_version}."
    mkdir -p "${BUILD_DIR}"
    wget -O- "https://www.kernel.org/pub/linux/kernel/v${major_version}.x/linux-$kernel_version.tar.gz" \
        | tar --strip-components=1 -xzf - -C "${BUILD_DIR}"
}

install_cos_linux_headers() {
    if grep -q CHROMEOS_RELEASE_VERSION "${LSB_FILE}" > /dev/null; then
        BUILD_ID=$(awk '/CHROMEOS_RELEASE_VERSION *= */ { gsub(/^CHROMEOS_RELEASE_VERSION *= */, ""); print }' "${LSB_FILE}")
        BUILD_DIR="/linux-lakitu-${BUILD_ID}"
        SOURCES_DIR="${TARGET_DIR}/linux-lakitu-${BUILD_ID}"

        if [[ ! -e "${SOURCES_DIR}/.installed" ]]; then
            echo "Installing kernel headers for COS build ${BUILD_ID}"
            fetch_cos_linux_sources
            generate_headers
            rm -rf "${TARGET_DIR}${BUILD_DIR}"
            mv "${BUILD_DIR}" "${TARGET_DIR}"
            touch "${SOURCES_DIR}/.installed"
        fi
    fi
}

install_generic_linux_headers() {
    BUILD_DIR="/linux-headers-${KERNEL_VERSION}"
    SOURCES_DIR="${TARGET_DIR}/linux-headers-${KERNEL_VERSION}"

    if [[ ! -e "${SOURCES_DIR}/.installed" ]]; then
        echo "Installing kernel headers for generic kernel"
        fetch_generic_linux_sources
        generate_headers
        rm -rf "${TARGET_DIR}${BUILD_DIR}"
        mv "${BUILD_DIR}" "${TARGET_DIR}"
        touch "${SOURCES_DIR}/.installed"
    fi
}

install_headers() {
    distro="$(awk '/^NAME *= */ { gsub(/^NAME *= */, ""); print }' "${OS_RELEASE_FILE}")"

    case $distro in
        *"Container-Optimized OS"*)
            install_cos_linux_headers
            HEADERS_TARGET="${SOURCES_DIR}"
            ;;
        *)
            echo "WARNING: Cannot find distro-specific headers for ${distro}. Fetching generic headers."
            install_generic_linux_headers
            HEADERS_TARGET="${SOURCES_DIR}"
            ;;
    esac
}

check_headers() {
    modules_path="$1"
    arch="$(uname -m)"
    kdir="${modules_path}/${KERNEL_VERSION}"

    [[ "${arch}" == "x86_64" ]] && arch="x86"

    [[ ! -e "${kdir}" ]] && return 1
    [[ ! -e "${kdir}/source" ]] && [[ ! -e "${kdir}/build" ]] && return 1

    header_dir="$([[ -e "${kdir}/source" ]] && echo "${kdir}/source" || echo "${kdir}/build")"

    [[ ! -e "${header_dir}/include/linux/kconfig.h" ]] && return 1
    [[ ! -e "${header_dir}/include/generated/uapi" ]] && return 1
    [[ ! -e "${header_dir}/arch/${arch}/include/generated/uapi" ]] && return 1

    return 0
}

if [[ ! -e "/lib/modules/.installed" ]]; then
    if check_headers "${HOST_MODULES_DIR}"; then
        HEADERS_TARGET="${HOST_MODULES_DIR}/source"
    else
        install_headers
    fi

    mkdir -p "/lib/modules/${KERNEL_VERSION}"
    ln -sf "${HEADERS_TARGET}" "/lib/modules/${KERNEL_VERSION}/source"
    ln -sf "${HEADERS_TARGET}" "/lib/modules/${KERNEL_VERSION}/build"
    touch "/lib/modules/.installed"
    exit 0
else
    echo "Headers already installed"
    exit 0
fi

if [[ "$(sysctl -n kernel.kptr_restrict)" != "0" ]]; then
    # If this is not zero, it can interfere with most of the tools
    sysctl -w kernel.kptr_restrict=0
fi
