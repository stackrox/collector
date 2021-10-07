#!/bin/bash

log() { echo "$*" >&2; }

function get_os_release_value() {
    local key="$1"
    local os_release_file="/host/etc/os-release"
    if [[ ! -f "$os_release_file" ]]; then
        os_release_file="/host/usr/lib/os-release"
    fi
    if [[ -f "$os_release_file" ]]; then
        while IFS="=" read -r var value; do
            if [[ "$key" == "$var" ]]; then
                # remove quotes
                local trimmed_value
                trimmed_value="${value%\"}"
                trimmed_value="${trimmed_value#\"}"
                echo "$trimmed_value"
            fi
        done < "$os_release_file"
    fi
}

function get_distro() {
    local distro
    distro=$(get_os_release_value 'PRETTY_NAME')
    if [[ -z "$distro" ]]; then
      echo "Linux"
    fi
    echo "$distro"
}

function test {
    "$@"
    local status=$?
    if [[ $status -ne 0 ]]; then
        log "Error with $1"
        exit $status
    fi
    return $status
}

function remove_module() {
    local module_name="$1"
    if lsmod | grep -q "$module_name"; then
        log "Collector kernel module has already been loaded."
        log "Removing so that collector can insert it at startup."
        test rmmod "$module_name"
    fi
}

function download_kernel_object() {
    local KERNEL_OBJECT
    local KERNEL_OBJECT="$1"
    local OBJECT_PATH="$2"
    local OBJECT_TYPE="kernel module"
    if [[ "${KERNEL_OBJECT##*.}" == "o" ]]; then
      OBJECT_TYPE="eBPF probe"
    fi

    local filename_gz="${OBJECT_PATH}.gz"
    local curl_opts=(
        -sS --retry 30 --retry-connrefused --retry-delay 1 --retry-max-time 60
        --connect-timeout 2
        -f -L -w "HTTP Status Code %{http_code}"
        -o "${filename_gz}"
    )

    if [[ ! -f "${filename_gz}" && -n "$GRPC_SERVER" ]]; then
        local connect_to_opts=()
        local server_port="${GRPC_SERVER##*:}"
        if [[ "$server_port" == "$GRPC_SERVER" ]]; then
            echo >&2 "GRPC_SERVER env var must specify the port"
            exit 1
        fi

        local sni_port="${SNI_HOSTNAME##*:}"
        if [[ "$sni_port" != "$SNI_HOSTNAME" ]]; then
            echo >&2 "SNI_HOSTNAME env var must NOT specify the port"
            exit 1
        fi

        local server_hostname="${GRPC_SERVER%:"$server_port"}"
        if [[ "$SNI_HOSTNAME" != "$server_hostname" ]]; then
            server_hostname="${SNI_HOSTNAME}"
            connect_to_opts=(--connect-to "${SNI_HOSTNAME}:${server_port}:${GRPC_SERVER}")
        fi

        local url="https://${server_hostname}:${server_port}/kernel-objects/${module_version}/${KERNEL_OBJECT}.gz"

        curl "${curl_opts[@]}" "${connect_to_opts[@]}" \
            --cacert /run/secrets/stackrox.io/certs/ca.pem \
            --cert /run/secrets/stackrox.io/certs/cert.pem \
            --key /run/secrets/stackrox.io/certs/key.pem \
            "$url"
        if [[ $? -ne 0 ]]; then
            rm -f "${filename_gz}" 2>/dev/null
            log "Unable to download from ${url}"
        fi
    fi
    if [[ ! -f "${filename_gz}" && -n "${MODULE_URL}" ]]; then
        local url="${MODULE_URL}/${KERNEL_OBJECT}.gz"
        curl "${curl_opts[@]}" "$url"
        if [[ $? -ne 0 ]]; then
            rm -f "${filename_gz}" 2>/dev/null
            log "Unable to download from ${url}"
            return 1
        fi
    fi

    if [[ ! -f "${filename_gz}" ]]; then
        return 1
    fi

    if ! gzip -d --keep "${filename_gz}"; then
        rm -f "${filename_gz}" 2>/dev/null
        rm -f "${OBJECT_PATH}" 2>/dev/null
        log "Failed to decompress ${OBJECT_TYPE} ${KERNEL_OBJECT} after download and removing from local storage."
        return 1
    fi

    log "Downloaded ${OBJECT_TYPE} ${KERNEL_OBJECT}."
    return 0
}

function find_kernel_object() {
    local KERNEL_OBJECT="$1"
    local OBJECT_PATH="$2"
    local EXPECTED_PATH="/kernel-modules/${KERNEL_OBJECT}"
    local OBJECT_TYPE="kernel module"
    if [[ "${KERNEL_OBJECT##*.}" == "o" ]]; then
      OBJECT_TYPE="eBPF probe"
    fi

    if [[ -f "${EXPECTED_PATH}.gz" ]]; then
      if ! gunzip -c "${EXPECTED_PATH}.gz" >"${OBJECT_PATH}"; then
        rm -f "${OBJECT_PATH}" 2>/dev/null
        log "Failed to decompress ${OBJECT_TYPE}, removing from local storage."
        return 1
      fi
    elif [ -f "$EXPECTED_PATH" ]; then
      cp "$EXPECTED_PATH" "$OBJECT_PATH"
    else
      log "Local storage does not contain ${OBJECT_TYPE} ${KERNEL_OBJECT}."
      return 1
    fi

    log "Local storage contains ${OBJECT_TYPE} ${KERNEL_OBJECT}."
    return 0
}

# Kernel version >= 4.14, or running on RHEL 7.6 family kernel with backported eBPF
function kernel_supports_ebpf() {
    if rhel76_host; then
      return 0
    fi
    if [[ ${KERNEL_MAJOR} -lt 4 || ( ${KERNEL_MAJOR} -eq 4 && ${KERNEL_MINOR} -lt 14 ) ]]; then
        return 1
    fi
    return 0
}

function cos_host() {
    if [[ "$OS_ID" == "cos" ]] && [[ -n "$OS_BUILD_ID" ]]; then
        return 0
    fi
    return 1
}

function coreos_host() {
    if [[ "$OS_ID" == "coreos" ]]; then
        return 0
    fi
    return 1
}

function dockerdesktop_host() {
    if [[ "$OS_DISTRO" == "Docker Desktop" ]]; then
        return 0
    fi
    return 1
}

function get_ubuntu_backport_version() {
    if [[ "$OS_ID" == "ubuntu" ]]; then
        local uname_version
        uname_version="$(uname -v)"
        # Check uname for backport version 16.04
        if [[ "${uname_version}" == *"~16.04"* ]]; then
            echo "~16.04"
    elif [[ "${uname_version}" == *"~20.04"* ]]; then
            echo "~20.04"
        fi
    fi
}

# RHEL 7.6 family detection: id=="rhel"||"centos", and kernel build id at least 957
# Assumption is that RHEL 7.6 will continue to use kernel 3.10
function rhel76_host() {
    if [[ "$OS_ID" == "rhel" || "$OS_ID" == "centos" ]] && [[ "$KERNEL_VERSION" == *".el7."* ]]; then
        if [[ ${KERNEL_MAJOR} -eq 3 && ${KERNEL_MINOR} -eq 10 ]]; then
            # Extract build id: 3.10.0-957.10.1.el7.x86_64 -> 957
            local kernel_build_id
            kernel_build_id=$(echo "$KERNEL_VERSION" | cut -d. -f3 | cut -d- -f2)
            if [[ ${kernel_build_id} -ge 957 ]] && [[ ${kernel_build_id} -lt 1062 ]] ; then
                return 0
            fi
        fi
    fi
    return 1
}

function collection_method_module() {
    local method
    method="$(echo "$COLLECTION_METHOD" | tr '[:upper:]' '[:lower:]')"
    if [[ "$method" == "kernel_module" || "$method" == "kernel-module" ]]; then
        return 0
    fi
    return 1
}

function collection_method_ebpf() {
    local method
    method="$(echo "$COLLECTION_METHOD" | tr '[:upper:]' '[:lower:]')"
    if [[ "$method" == "ebpf" ]]; then
        return 0
    fi
    return 1
}

exit_with_error() {
    log ""
    log "Please provide this complete error message to StackRox support."
    log "This program will now exit and retry when it is next restarted."
    log ""
    exit 1
}

function clean_up() {
    log "collector pid to be stopped is $PID"
    kill -TERM "$PID"; wait "$PID"
}

function gpl_notice() {
    log ""
    log "This product uses kernel module and ebpf subcomponents licensed under the GNU"
    log "GENERAL PURPOSE LICENSE Version 2 outlined in the /kernel-modules/LICENSE file."
    log "Source code for the kernel module and ebpf subcomponents is available upon"
    log "request by contacting support@stackrox.com."
    log ""
}

function get_kernel_object() {
    local kernel_version="$1"

    local alternate_download
    alternate_download="$(echo "$ROX_COLLECTOR_ALT_PROBE_DOWNLOAD" | tr '[:upper:]' '[:lower:]')"

    # Find built-in or download kernel module
    if collection_method_module; then
      local module_name="collector"
      local module_path="/module/${module_name}.ko"
      local kernel_module="${module_name}-${kernel_version}.ko"

      if ! find_kernel_object "$kernel_module" "$module_path"; then
        if [[ "$alternate_download" == "true" ]]; then
          log "Kernel object will be downloaded by the collector binary"

          # The collector program will insert the kernel module upon startup.
          remove_module "$module_name"

          return 0
        fi

        if ! download_kernel_object "${kernel_module}" "${module_path}" || [[ ! -f "$module_path" ]]; then
          return 1
        fi
      fi

      if [[ -f "$module_path" ]]; then
        chmod 0444 "$module_path"
      else
        log "Did not find kernel module for kernel version $kernel_version."
        return 1
      fi

      # The collector program will insert the kernel module upon startup.
      remove_module "$module_name"

    # Find built-in or download ebpf probe
    elif collection_method_ebpf; then
      if kernel_supports_ebpf; then
        local probe_name="collector-ebpf"
        local probe_path="/module/${probe_name}.o"
        local kernel_probe="${probe_name}-${kernel_version}.o"

        if ! find_kernel_object "${kernel_probe}" "${probe_path}"; then
          if [[ "$alternate_download" == "true" ]]; then
            log "Kernel object will be downloaded by the collector binary"
            return 0
          fi

          if ! download_kernel_object "${kernel_probe}" "${probe_path}" || [[ ! -f "$probe_path" ]]; then
            return 1
          fi
        fi

        if [[ -f "$probe_path" ]]; then
          chmod 0444 "$probe_path"
        else
          log "Did not find ebpf probe for kernel version $kernel_version."
          return 1
        fi
      else
        log "Kernel ${kernel_version} doesn't support eBPF"
        return 1
      fi
    fi
    return 0
}

function main() {
    
    # Get the host kernel version (or user defined env var)
    [ -n "$KERNEL_VERSION" ] || KERNEL_VERSION="$(uname -r)"

    # Get the kernel version
    KERNEL_MAJOR=$(echo "$KERNEL_VERSION" | cut -d. -f1)
    KERNEL_MINOR=$(echo "$KERNEL_VERSION" | cut -d. -f2)
    
    # Get and export the node hostname from Docker, 
    # and export because this env var is read by collector
    export NODE_HOSTNAME="$(cat /host/proc/sys/kernel/hostname)"
    
    # Export SNI_HOSTNAME and default it to sensor.stackrox
    export SNI_HOSTNAME="${SNI_HOSTNAME:-sensor.stackrox}"

    # Get the linux distribution and BUILD_ID and ID to identify kernel version (COS or RHEL)
    OS_DISTRO="$(get_distro)"
    OS_BUILD_ID="$(get_os_release_value 'BUILD_ID')"
    OS_ID="$(get_os_release_value 'ID')"
    
    # Print node info
    log "Collector Version: ${COLLECTOR_VERSION}"
    log "Hostname: ${NODE_HOSTNAME}"
    log "OS: ${OS_DISTRO}"
    log "Kernel Version: ${KERNEL_VERSION}"
    
    local module_version
    module_version="$(cat /kernel-modules/MODULE_VERSION.txt)"
    if [[ -n "$module_version" ]]; then
        log "Module Version: $module_version"
        if [[ -n "$MODULE_DOWNLOAD_BASE_URL" ]]; then
            MODULE_URL="${MODULE_DOWNLOAD_BASE_URL}/${module_version}"
        fi
    fi

    # Special case kernel version if running on COS
    if cos_host ; then
        # remove '+' from end of kernel version 
        KERNEL_VERSION="${KERNEL_VERSION%+}-${OS_BUILD_ID}-${OS_ID}"
    fi

    # Special case kernel version if running on Docker Desktop
    if dockerdesktop_host ; then
        banner_ts="$(uname -a | awk -F'SMP ' '{print $2}' | awk -F'x86_64' '{print $1}')"
        kernel_version_ts="$(date '+%Y-%m-%d-%H-%M-%S' -d "${banner_ts}")"
        KERNEL_VERSION="${KERNEL_VERSION%-linuxkit}-dockerdesktop-${kernel_version_ts}"
        if collection_method_ebpf; then
            log "Warning: ${OS_DISTRO} does not support ebpf, switching to kernel module based collection"
            COLLECTION_METHOD="KERNEL_MODULE"
        fi
    fi

    mkdir -p /module
    
    # Backwards compatability for releases older than 2.4.20
    # COLLECTION_METHOD should be provided
    if [[ -z "$COLLECTION_METHOD" ]]; then
      log "Collector configured without environment variable (default: COLLECTION_METHOD=kernel_module)"
      export COLLECTION_METHOD="KERNEL_MODULE"
    fi
    
    # Handle invalid collection method setting
    if ! collection_method_ebpf && ! collection_method_module ; then
      log "Error: Collector configured with invalid value: COLLECTION_METHOD=${COLLECTION_METHOD}"
      exit_with_error
    fi
    
    # Handle attempt to run kernel module collection on COS host
    if cos_host && collection_method_module ; then
      log "Error: ${OS_DISTRO} does not support third-party kernel modules"
      if kernel_supports_ebpf; then
        log "Warning: Switching to eBPF based collection, please configure RUNTIME_SUPPORT=ebpf"
        COLLECTION_METHOD="EBPF"
      else
        exit_with_error
      fi
    fi
    
    # Handle attempt to run ebpf collection on host with kernel that does not support ebpf
    if collection_method_ebpf && ! kernel_supports_ebpf; then
      if cos_host; then 
        log "Error: ${OS_DISTRO} does not support third-party kernel modules or the required eBPF features."
        exit_with_error
      else
        log "Error: ${OS_DISTRO} ${KERNEL_VERSION} does not support ebpf based collection."
        log "Warning: Switching to kernel module based collection, please configure RUNTIME_SUPPORT=kernel-module"
        COLLECTION_METHOD="KERNEL_MODULE"
      fi
    fi

    kernel_versions=()
    # Add backport kernel version if running on Ubuntu backport kernel
    ubuntu_backport_version="$(get_ubuntu_backport_version)"
    if [[ -n "$ubuntu_backport_version" ]]; then
        kernel_versions+=("${KERNEL_VERSION}${ubuntu_backport_version}")
    fi
    kernel_versions+=("${KERNEL_VERSION}")

    success=0
    for kernel_version in "${kernel_versions[@]}"; do
      if get_kernel_object "${kernel_version}"; then
        success=1
        break
      fi
    done
    if (( ! success )); then
      exit_with_error
    fi

    export KERNEL_CANDIDATES="${kernel_versions[@]}"

    # Print GPL notice after probe is downloaded or verified to be present
    gpl_notice
    
    # Uncomment this to enable generation of core for Collector
    # echo '/core/core.%e.%p.%t' > /proc/sys/kernel/core_pattern
    
    # Remove "/bin/sh -c" from arguments
    shift;shift
    log "Starting StackRox Collector..."
    # Signal handler for SIGTERM
    trap 'clean_up' TERM QUIT INT
    eval exec "$@" &
    PID=$!
    wait $PID
    status=$?

    if collection_method_module; then
        remove_module "$module_name"
    fi
    exit $status
}

main "$@"
