#!/usr/bin/env bash
set -eo pipefail

collection_method=$1
vm_type=$2
image_family=$3
serial_output_basedir=$4
BUILD_NUM=$5

export TEST_NAME="${collection_method}-${vm_type}-${image_family}"
export GCLOUD_INSTANCE="collector-ci-${TEST_NAME}-${BUILD_NUM}"

mkdir -p "$serial_output_basedir/serial-output"

gcloud compute instances get-serial-port-output "$GCLOUD_INSTANCE" > "$serial_output_basedir"/serial-output/"$GCLOUD_INSTANCE"-serial-output.logs
