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
    
    # Get and export the node hostname from Docker, 
    # and export because this env var is read by collector
    export NODE_HOSTNAME="$(cat /host/proc/sys/kernel/hostname)"
    
    # Export SNI_HOSTNAME and default it to sensor.stackrox
    export SNI_HOSTNAME="${SNI_HOSTNAME:-sensor.stackrox}"

    # Get the linux distribution and BUILD_ID and ID to identify kernel version (COS or RHEL)
    OS_DISTRO="$(get_distro)"
    
    # Print node info
    log "Collector Version: ${COLLECTOR_VERSION}"
    log "Hostname: ${NODE_HOSTNAME}"
    log "OS: ${OS_DISTRO}"
    log "Kernel Version: ${KERNEL_VERSION}"

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

    exit $status
}

main "$@"
