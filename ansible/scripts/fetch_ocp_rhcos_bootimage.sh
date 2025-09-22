#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <ocp-version> [<architectures.path>]"
    exit 1
fi

OCP_VERSION=$1

# Json path with architecture and image name, e.g., x86_64.images.gcp.name or s390x.artifacts.ibmcloud.release
JSONPATH=${2:-"x86_64.images.gcp.name"}

URL="https://raw.githubusercontent.com/openshift/installer/release-${OCP_VERSION}/data/data/coreos/rhcos.json"

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
