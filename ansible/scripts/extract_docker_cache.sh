#!/bin/bash
# based on https://github.com/reproducible-containers/buildkit-cache-dance

set -e

usage() {
    echo "Usage: $0 <cache_destination_directory> <target_path_inside_container>"
    exit 1
}

if [ "$#" -ne 2 ]; then
    usage
fi

CACHE_DEST="$1"
TARGET_PATH="$2"

CCACHE_ARCHIVE="docker.tar.gz"

SCRATCH_DIR=$(mktemp -d)
SCRATCH_ARCHIVE=$(mktemp)
# shellcheck disable=SC2064
trap "rm -rf ${SCRATCH_DIR}; rm -f ${SCRATCH_ARCHIVE}" EXIT

# Timestamp to bust cache
date -Iseconds > "${SCRATCH_DIR}/buildstamp"

cat << EOF > "${SCRATCH_DIR}/Dockerfile.extract"
FROM busybox:1
COPY buildstamp buildstamp
RUN --mount=type=cache,target=${TARGET_PATH} \\
    mkdir -p /var/docker-cache/ \\
    && cp -p -R ${TARGET_PATH}/. /var/docker-cache/ || true
EOF

echo "Generated Dockerfile.extract"
cat "${SCRATCH_DIR}/Dockerfile.extract"

# Build the image and load it into Docker
docker buildx build -f "${SCRATCH_DIR}/Dockerfile.extract" --tag cache:extract --load "${SCRATCH_DIR}"
docker images

# Remove any existing cache-container
docker rm -f cache-container || true

# Create a container from cache:extract
docker create --name cache-container cache:extract
docker ps

# Extract the cache from the container
docker cp -L cache-container:/var/docker-cache - | tar -H posix -x -C "${SCRATCH_DIR}"
ls "${SCRATCH_DIR}"

# Compress the cache from the container
(cd "${SCRATCH_DIR}/docker-cache" && chmod -R 777 . && tar czf "${SCRATCH_ARCHIVE}" .)

# Move the cache into its dest
rm -f "${CACHE_DEST}/${CCACHE_ARCHIVE}"
mv "${SCRATCH_ARCHIVE}" "${CACHE_DEST}/${CCACHE_ARCHIVE}"
chmod 666 "${CACHE_DEST}/${CCACHE_ARCHIVE}"

echo "Docker cache extraction completed successfully."
