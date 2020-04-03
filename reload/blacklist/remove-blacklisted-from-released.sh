#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)"

while IFS='' read -r line || [[ -n "$line" ]]; do
    [[ -n "$line" ]] || continue
    collector_version="$line"

    #collector_repo="collector.stackrox.io"
    collector_repo="docker.io/stackrox"
    collector_image="${collector_repo}/collector:${collector_version}"
    collector_image_rebuilt="${collector_repo}/collector:${collector_version}-rebuilt"
    docker pull "${collector_image}"
    docker build \
        -t "${collector_image_rebuilt}" \
        --build-arg collector_version="${collector_version}" "${DIR}"
    # sanity check
    docker run -i --rm --entrypoint /bin/sh "${collector_image_rebuilt}" <<EOF
set -e
cat /kernel-modules/MODULE_VERSION.txt
ls kernel-modules/collector-ebpf-4.19.*-coreos*.o.gz
ls kernel-modules/collector-ebpf-4.19.*-cos.o.gz
EOF

    docker images | grep "${collector_version}"

    docker tag "${collector_image_rebuilt}" "${collector_image}"

    #docker push "${collector_image}"

done < <(grep -v '^#' <"${DIR}/../../RELEASED_VERSIONS" | awk '{print $1}' | sort -r | uniq)
