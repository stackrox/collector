#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <ocp-version> [<architectures.path>] [<rhel-variant>]"
    exit 1
fi

OCP_VERSION=$1

# Json path with architecture and image name, e.g., x86_64.images.gcp.name or s390x.artifacts.ibmcloud.release
JSONPATH=${2:-"x86_64.images.gcp.name"}

# Optional RHEL variant (e.g., rhel-9, rhel-10) for OCP 4.22+ which splits
# RHCOS images into separate coreos-rhel-9.json and coreos-rhel-10.json files.
RHEL_VARIANT=${3:-""}

if [ -n "$RHEL_VARIANT" ]; then
    URL="https://raw.githubusercontent.com/openshift/installer/release-${OCP_VERSION}/data/data/coreos/coreos-${RHEL_VARIANT}.json"
else
    URL="https://raw.githubusercontent.com/openshift/installer/release-${OCP_VERSION}/data/data/coreos/rhcos.json"
fi

json_data=$(curl --retry 5 --retry-delay 2 -sS "$URL")
if [ -z "$json_data" ]; then
    echo "Failed to fetch JSON data from URL: $URL"
    exit 1
fi

image_name=$(echo "$json_data" | jq -r ".architectures.${JSONPATH}")
if [ "$image_name" == "null" ]; then
    echo "Failed to parse JSON data or path does not exist"
    exit 1
fi
echo "$image_name"
