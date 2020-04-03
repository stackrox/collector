#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)"

while IFS='' read -r line || [[ -n "$line" ]]; do
    [[ -n "$line" ]] || continue
    collector_version="$line"

    collector_repo="collector.stackrox.io/collector"
    #collector_repo="docker.io/stackrox/collector"
    collector_image="${collector_repo}:${collector_version}-latest"
    collector_image_rebuilt="${collector_repo}:${collector_version}-rebuilt"
    docker pull "${collector_image}"
    docker build \
        -t "${collector_image_rebuilt}" \
        --build-arg collector_version="${collector_version}" \
        --build-arg collector_repo="${collector_repo}" \
        "${DIR}"
    
    # sanity check
    echo "Sanity check 4.19 eBPF in latest"
    docker run -i --rm --entrypoint /bin/sh "${collector_image}" <<EOF
ls kernel-modules/collector-ebpf-4.19.*.o.gz
EOF

    # sanity check
    echo "Sanity check 4.19 eBPF in rebuilt"
    docker run -i --rm --entrypoint /bin/sh "${collector_image_rebuilt}" <<EOF
ls kernel-modules/collector-ebpf-4.19.*.o.gz
EOF

    docker images | grep "${collector_version}"

    echo "Tagging ${collector_image_rebuilt} -> ${collector_image}"
    docker tag "${collector_image_rebuilt}" "${collector_image}"

    docker images | grep "${collector_version}"

    echo "Pushing rebuilt ${collector_image}"
    docker push "${collector_image}"

done < <(grep -v '^#' <"${DIR}/../../RELEASED_VERSIONS" | awk '{print $1}' | sort -r | uniq)
