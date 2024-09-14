#!/bin/bash
# based on https://github.com/reproducible-containers/buildkit-cache-dance

set -e

usage() {
    echo "Usage: $0 <cache_source_directory> <target_path_inside_container>"
    exit 1
}

if [ "$#" -ne 2 ]; then
    usage
fi

CACHE_SOURCE="$1"
TARGET_PATH="$2"

if [ ! -d "$CACHE_SOURCE" ]; then
    echo "Error: Cache source directory '$CACHE_SOURCE' does not exist."
    exit 1
fi

CCACHE_ARCHIVE="docker.tar.gz"

SCRATCH_DIR=$(mktemp -d)
# shellcheck disable=SC2064
trap "rm -rf ${SCRATCH_DIR}" EXIT

# Timestamp to bust cache
date -Iseconds > "${CACHE_SOURCE}/buildstamp"

cat << EOF > "${SCRATCH_DIR}/Dockerfile.inject"
FROM busybox:1
COPY buildstamp buildstamp
RUN --mount=type=cache,target=${TARGET_PATH} \\
    --mount=type=bind,source=.,target=/var/docker-cache \\
    tar -xzf /var/docker-cache/${CCACHE_ARCHIVE} -C ${TARGET_PATH} || true
EOF

echo "Generated Dockerfile.inject"
cat "${SCRATCH_DIR}/Dockerfile.inject"

# Inject ccache into Docker cache
cd "$CACHE_SOURCE"
docker buildx build -f "${SCRATCH_DIR}/Dockerfile.inject" --tag cache:inject .

echo "Cache injection completed successfully."
