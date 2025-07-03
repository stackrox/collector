#!/usr/bin/env bash

set -euo pipefail

function usage() {
    echo "$0 [OPTIONS]"
    echo ""
    echo "[OPTIONS]"
    echo " -a, --arch"
    echo "    Architecture wanted for the listed images"
    echo " -p, --project"
    echo "    Project the image is hosted in"
    echo " -f, --family"
    echo "    Regular expression for the image family to be listed"
}

TEMP=$(getopt -o 'ha:p:f:' -l 'help,arch,project,family' -n "$0" -- "$@")

# shellcheck disable=SC2181
if [ $? -ne 0 ]; then
    exit 1
fi

eval set -- "$TEMP"
unset TEMP

arch=""
family=""
project=""
filter=""
using_regex=0
using_aip=0

while true; do
    case "${1:-}" in
        '-h' | '--help')
            usage
            exit 0
            ;;

        '-a' | '--arch')
            arch="architecture=$2"
            using_aip=1
            shift 2
            ;;

        '-p' | '--project')
            project="selfLink=/$2/"
            using_aip=1
            shift 2
            ;;

        '-f' | '--family')
            family="family ~ \"$2\""
            using_regex=1
            shift 2
            ;;

        '--')
            shift
            break
            ;;
    esac
done

if ((using_regex != 0 && using_aip != 0)); then
    echo >&2 "Mixing regex and AIP-160 is not supported in gcloud filters"
    echo >&2 "see: https://cloud.google.com/compute/docs/reference/rest/v1/images/list"
    exit 1
elif ((using_regex != 0)); then
    filter="$family"
elif ((using_aip != 0)); then
    if [[ -n "$project" ]]; then
        filter="$project"
        if [[ -n "$arch" ]]; then
            filter="$filter AND $arch"
        fi
    elif [[ -n "$arch" ]]; then
        filter="$arch"
    fi
fi

gcloud compute images list \
    --format='json(family)' \
    --filter="$filter" \
    --limit 10 | jq -cM '[ .[].family ]'
