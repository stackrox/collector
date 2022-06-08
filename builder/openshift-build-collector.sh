
#!/usr/bin/env bash
set -eo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

ls; echo; echo

cp -r "${ROOT}" /tmp/collector

make -C /tmp/collector/collector cmake-build/collector

export DISABLE_PROFILING="true"
export SRC_ROOT_DIR=/tmp/collector
/build-collector.sh
