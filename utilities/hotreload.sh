#!/usr/bin/env bash

set -euo pipefail

NAMESPACE="${NAMESPACE:-stackrox}"
REVERT=0

# The following line allows to symlink the script somewhere in PATH for
# easier use, has no effect if run directly.
REAL_SCRIPT="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd -- "$( dirname -- "${REAL_SCRIPT}")" &> /dev/null && pwd)"
COLLECTOR_PATH="$(realpath "${SCRIPT_DIR:-}/..")"

function usage() {
    cat << EOF
$(basename "$0") [OPTIONS]

Configure a running collector pod to use a local binary.

OPTIONS:
    -h, --help
        Show this help.
    -r, --revert
        Remove any configuration added by this script.
    -p, --path
        Override the path to the collector repo.
    -n, --namespace
        Set the namespace the collector daemonset is deployed to.
        Default: stackrox
EOF
}

function die() {
    echo >&2 "$1"
    exit 1
}

function check_command() {
    if ! command -v "$1" &> /dev/null; then
        die "$1 not found. Make sure it is in your path"
    fi
}

function krox() {
    kubectl -n "${NAMESPACE}" "$@"
}

function patch_hotreload() {
    cat << EOF | jq -Mc \
        --arg hostPath "$COLLECTOR_PATH" \
        '.spec.template.spec.volumes.[0].hostPath.path |= $hostPath'
{
  "spec": {
    "template": {
      "spec": {
        "containers": [
          {
            "name": "collector",
            "command": ["/host/src/cmake-build/collector/collector"],
            "volumeMounts": [
              {
                "mountPath": "/host/src",
                "name": "collector-src",
                "readOnly": true
              }
            ]
          }
        ],
        "volumes": [
          {
            "hostPath": {
              "path": "\$hostPath",
              "type": ""
            },
            "name": "collector-src"
          }
        ]
      }
    }
  }
}
EOF
}

function revert_command() {
    cat << EOF | jq -Mc
{
  "spec": {
    "template": {
      "spec": {
        "containers": [
          {
            "name": "collector",
            "command": ["collector"]
          }
        ]
      }
    }
  }
}
EOF
}

function remove_mount() {
     jq -Mcn \
        --arg container "$1" \
        --arg volume "$2" \
        '[{"op":"remove","path":"/spec/template/spec/containers/\($container)/volumeMounts/\($volume)"}]'
}

function remove_volume() {
     jq -Mcn \
         --arg volume "$1" \
         '[{"op":"remove","path":"/spec/template/spec/volumes/\($volume)"}]'
}

function find_index() {
    local element="$1"
    local filter="$2"
    ret="$(krox get -o json ds/collector | jq "$element | map($filter) | index(true)")"
    if [[ "$ret" == "null" ]]; then
        die "Filter failed for '$filter'"
    fi
    echo "$ret"
}

function add_hotreload() {
    krox patch ds collector -p "$(patch_hotreload)"
}

function revert_config() {
    local collector_idx
    collector_idx="$(find_index ".spec.template.spec.containers" '.name == "collector"')"
    local mount_idx
    mount_idx="$(find_index ".spec.template.spec.containers.[$collector_idx].volumeMounts" '.name == "collector-src"')"
    local volume_idx
    volume_idx="$(find_index ".spec.template.spec.volumes" '.name == "collector-src"')"

    krox patch ds collector -p "$(revert_command)"
    krox patch ds collector --type=json -p "$(remove_mount "$collector_idx" "$mount_idx")"
    krox patch ds collector --type=json -p "$(remove_volume "$volume_idx")"
}

check_command jq
check_command kubectl

TEMP=$(getopt -o 'hrp:n:' -l 'help,revert,path:,namespace:' -n "$0" -- "$@")

# shellcheck disable=SC2181
if [ $? -ne 0 ]; then
    exit 1
fi

eval set -- "$TEMP"
unset TEMP

while true; do
    case "${1:-}" in
        '-h' | '--help')
            usage
            exit 0
            ;;

        '-r' | '--hotreload')
            REVERT=1
            shift
            ;;

        '-p' | '--path')
            COLLECTOR_PATH="$2"
            shift 2
            ;;

        '-n' | '--namespace')
            NAMESPACE="$2"
            shift 2
            ;;

        '--')
            shift
            break
            ;;
    esac
done

if ((REVERT)); then
    revert_config
else
    add_hotreload
fi
