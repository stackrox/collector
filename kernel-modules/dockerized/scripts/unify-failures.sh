#!/usr/bin/env bash

set -euo pipefail

cp -r /FAILURES/. /tmp/dockerized-failures || true

shopt -s nullglob
cd /tmp/dockerized-failures
failure_files=(*/*/*.log)

for failure_file in "${failure_files[@]}"; do
	if [[ "$failure_file" =~ ^([^/]+)/([^/]+)/([^/]+)\.log$ ]]; then
		module_version="${BASH_REMATCH[1]}"
		kernel_version="${BASH_REMATCH[2]}"
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
