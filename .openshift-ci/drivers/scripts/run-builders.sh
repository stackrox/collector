#! /usr/bin/env bash

set -eou pipefail

shopt -s nullglob

export OSCI_RUN=1

copy_sources() {
    local shard
    shard=$(basename "$1")

    # Create a copy of the sources for this shard
    mkdir -p "/kobuild-tmp/shards/$shard"
    cp -r /kobuild-tmp/versions-src/* "/kobuild-tmp/shards/$shard/"
}

run_builder() {
    local shard=$1

    export CURRENT_SHARD
    CURRENT_SHARD="$(basename "$shard")"
    if [[ "$DISTRO" != "rhel7" ]]; then
        (/scripts/compile.sh < "$shard") &
    else
        (scl enable llvm-toolset-7.0 /scripts/compile.sh < "$shard") &
    fi
}

# Create empty directories, in case no builds are necessary
mkdir -p /built-drivers
mkdir -p /FAILURES

# Run builders in parallel for every shard and wait for them to finish
for shard in "$SHARDS_DIR"/*; do
    copy_sources "$shard"

    run_builder "$shard"
done

wait
