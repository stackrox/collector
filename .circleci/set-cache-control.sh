#!/bin/bash
set -eu

# This function documents how we can set cache-control on every layer blob
# across the registry backing store. Normally we would not need this since we
# update the cache-control setting on each layer after the layers are pushed.
#
# Empirical data from 20210428:
# * Full sweep across "gs://sr-registry-storage" took 123s to update 17k objects
# * Full sweep across "gs://sr-registry-storage-collector" took 324s to update 35k objects
function set_cache_control_full_sweep {
  gsutil -m setmeta -h "Cache-control:public, max-age=31536000" \
    "gs://sr-registry-storage/docker/registry/v2/blobs/sha256/**"
  gsutil -m setmeta -h "Cache-control:public, max-age=31536000" \
    "gs://sr-registry-storage-collector/docker/registry/v2/blobs/sha256/**"
}

function set_cache_control_all_layers {
  local image="$1"

  if [[ "$image" =~ collector ]]; then
    local bucket="sr-registry-storage-collector"
  else
    local bucket="sr-registry-storage"
  fi

  local layers=$(docker manifest inspect "$image" | jq -r ".layers[].digest")

  for layer in ${layers[*]}; do
    local sha256=${layer#sha256:}
    local prefix=${sha256:0:2}
    (set -x; gsutil -q setmeta -h "Cache-control:public, max-age=31536000" \
      "gs://$bucket/docker/registry/v2/blobs/sha256/$prefix/$sha256/data")
  done
}

function usage {
  cat <<EOF
Usage:
  set-cache-control.sh <image>

Examples:
  set-cache-control.sh "stackrox.io/main:3.0.58.0"
  set-cache-control.sh "stackrox.io/scanner:2.13.1"
  set-cache-control.sh "stackrox.io/scanner-db:2.13.1"
  set-cache-control.sh "collector.stackrox.io/collector:3.1.20"
EOF
  exit 1
}


# __MAIN__
image="${1:-}"
[[ -n "$image" ]] || usage
export DOCKER_CLI_EXPERIMENTAL="enabled"
set_cache_control_all_layers "$image"
