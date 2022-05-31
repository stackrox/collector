#!/usr/bin/env bash
set -eo pipefail

FAILURES_DIR=${FAILURES_DIR:-/tmp/failures}

shopt -s nullglob
cd "$FAILURES_DIR"
failure_files=(*/*/*.log)

for failure_file in "${failure_files[@]}"; do
    if [[ "$failure_file" =~ ^([^/]+)/([^/]+)/([^/]+)\.log$ ]]; then
        kernel_version="${BASH_REMATCH[1]}"
        module_version="${BASH_REMATCH[2]}"
        probe_type="${BASH_REMATCH[3]}"
        echo >&2 "============================================================================"
        echo >&2 "Failed to build ${probe_type} probe"
        echo >&2 "Module version: ${module_version}"
        echo >&2 "Kernel version: ${kernel_version}"
        echo >&2
        cat >&2 "$failure_file"
        echo >&2
        echo >&2
    fi
done

[[ "${#failure_files[@]}" == 0 ]]
