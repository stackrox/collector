#/usr/bin/env bash
set -eo pipefail

docker images | grep collector | grep "${COLLECTOR_VERSION}"
docker images | grep collector | grep "${COLLECTOR_VERSION}-slim"
