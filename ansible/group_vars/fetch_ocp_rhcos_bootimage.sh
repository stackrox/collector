#!/bin/bash

# Check if release version is provided as an argument
if [ -z "$1" ]; then
  echo "Usage: $0 <ocp-version> [<architectures.path>]"
  exit 1
fi

# Release version from the first argument
RELEASE_VERSION=$1

# Json path with architecture and image name
# e.g., s390x.artifacts.ibmcloud.release
JSONPATH=${2:-"x86_64.images.gcp.name"}


# URL of the JSON data
URL="https://raw.githubusercontent.com/openshift/installer/release-${RELEASE_VERSION}/data/data/coreos/rhcos.json"

json_data=$(curl -s "$URL")
if [ -z "$json_data" ]; then
  echo "Failed to fetch JSON data from URL: $URL"
  exit 1
fi

image_name=$(echo "$json_data" | jq -r ".architectures.${JSONPATH}")
if [ "$image_name" == "null" ]; then
  echo "Failed to parse JSON data or path does not exist"
  exit 1
fi

#echo "The image name for release $RELEASE_VERSION and architecture $ARCHITECTURE is: $image_name"
echo "$image_name"


