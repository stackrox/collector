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
                echo "$value"
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

    if [[ -z "${MODULE_URL}" ]]; then
        log "Collector is not configured to download the ${OBJECT_TYPE}"
        return 1
    fi
    local URL="${MODULE_URL}/${KERNEL_OBJECT}"
    local FILENAME_GZ="${OBJECT_PATH}.gz"

    # Attempt to download kernel object
    local HTTP_CODE
    HTTP_CODE=$(curl -w "%{http_code}" -L -s -o "$FILENAME_GZ" "${URL}.gz" 2>/dev/null)

    if [[ "$HTTP_CODE" != "200" ]] ; then
        log "Error downloading ${OBJECT_TYPE} ${KERNEL_OBJECT} (Error code: ${HTTP_CODE})"
        return 1
    fi

    gunzip "$FILENAME_GZ"
    log "Using downloaded ${OBJECT_TYPE} ${KERNEL_OBJECT}"
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
      gunzip -c "${EXPECTED_PATH}.gz" >"${OBJECT_PATH}"
    elif [ -f "$EXPECTED_PATH" ]; then
      cp "$EXPECTED_PATH" "$OBJECT_PATH"
    else
      log "Didn't find ${OBJECT_TYPE} ${KERNEL_OBJECT} built-in."
      return 1
    fi

    log "Using built-in ${OBJECT_TYPE} ${KERNEL_OBJECT}"
    return 0
}

# Kernel version >= 4.14, or running on RHEL 7.6 (with backported eBPF)
function kernel_supports_ebpf() {
    if rhel76_host; then
      return 1
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

function rhel76_host() {
    if [[ "$OS_ID" == "rhel" ]] && [[ "$OS_VERSION_ID" == "7.6" ]]; then
        return 0
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

function main() {

    # Get the host kernel version (or user defined env var)
    [ -n "$KERNEL_VERSION" ] || KERNEL_VERSION="$(uname -r)"
    
    # Get the kernel version and export because this env var is read by collector
    export KERNEL_MAJOR=""
    export KERNEL_MINOR=""
    KERNEL_MAJOR=$(echo "$KERNEL_VERSION" | cut -d. -f1)
    KERNEL_MINOR=$(echo "$KERNEL_VERSION" | cut -d. -f2)
    
    # Get and export the node hostname from Docker, 
    # and export because this env var is read by collector
    export NODE_HOSTNAME=""
    NODE_HOSTNAME=$(curl -s --unix-socket /host/var/run/docker.sock http://localhost/info | jq --raw-output .Name)
    
    # Get the linux distribution and OS_BUILD_ID and ID to identify COS kernel version
    # these are global vars used by other functions in this file
    export OS_DISTRO=""
    OS_DISTRO="$(get_distro)"
    export OS_BUILD_ID=""
    OS_BUILD_ID="$(get_os_release_value 'BUILD_ID')"
    export OS_VERSION_ID=""
    OS_VERSION_ID="$(get_os_release_value 'VERSION_ID')"
    export OS_ID=""
    OS_ID="$(get_os_release_value 'ID')"
    
    # Print node info
    log "Hostname: ${NODE_HOSTNAME}"
    log "OS: ${OS_DISTRO}"
    log "Kernel Version: ${KERNEL_VERSION}"
    
    local module_version
    module_version="$(cat /kernel-modules/MODULE_VERSION.txt)"
    if [[ -n "$module_version" ]]; then
        log "Collector Version: $module_version"
        if [[ -n "$MODULE_DOWNLOAD_BASE_URL" ]]; then
            MODULE_URL="${MODULE_DOWNLOAD_BASE_URL}/${module_version}"
        fi
    fi

    # Special case kernel version if running on COS
    if cos_host ; then
        # remove '+' from end of kernel version 
        KERNEL_VERSION="${KERNEL_VERSION%+}-${OS_BUILD_ID}-${ID}"
    fi
   
    mkdir -p /module
    
    # Backwards compatability for releases older than 2.4.20
    # COLLECTION_METHOD should be provided
    if [[ -z "$COLLECTION_METHOD" ]]; then
      export COLLECTION_METHOD=""
      local config_json_ebpf
      config_json_ebpf="$(echo "$COLLECTOR_CONFIG" | jq --raw-output .useEbpf)"
      if [[ "$config_json_ebpf" == "true" ]]; then
        COLLECTION_METHOD="EBPF"
      else
        COLLECTION_METHOD="KERNEL_MODULE"
      fi
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
    
    # Find built-in or download kernel module
    if collection_method_module; then
      local module_name="collector"
      local module_path="/module/${module_name}.ko"
      local kernel_module="${module_name}-${KERNEL_VERSION}.ko"

      if ! find_kernel_object "$kernel_module" "$module_path"; then
        if ! download_kernel_object "${kernel_module}" "${module_path}" || [[ ! -f "$module_path" ]]; then
          log "The kernel module may not have been compiled for version ${KERNEL_VERSION}."
        fi
      fi
      
      if [[ -f "$module_path" ]]; then
        chmod 0444 "$module_path"
      else
        log "Error: Failed to find kernel module for kernel version $KERNEL_VERSION."
        exit_with_error
      fi
    
      # The collector program will insert the kernel module upon startup.
      remove_module "$module_name"
    
    # Find built-in or download ebpf probe
    elif collection_method_ebpf; then
      if kernel_supports_ebpf; then
        local probe_name="collector-ebpf"
        local probe_path="/module/${probe_name}.o"
        local kernel_probe="${probe_name}-${KERNEL_VERSION}.o"
 
        if ! find_kernel_object "${kernel_probe}" "${probe_path}"; then
          if ! download_kernel_object "${kernel_probe}" "${probe_path}" || [[ ! -f "$probe_path" ]]; then
            log "The ebpf probe may not have been compiled for version ${KERNEL_VERSION}."
          fi
        fi
    
        if [[ -f "$probe_path" ]]; then
          chmod 0444 "$probe_path"
        else
          log "Error: Failed to find ebpf probe for kernel version $KERNEL_VERSION."
          exit_with_error
        fi
      else
        log "Error: Kernel ${KERNEL_VERSION} doesn't support eBPF"
        exit_with_error
      fi
    fi
    
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
    remove_module "$module_name"
}

main "$@"
