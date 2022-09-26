#!/usr/bin/env bash
set -eo pipefail

CI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
# shellcheck source=SCRIPTDIR/lib.sh
source "${CI_ROOT}/scripts/lib.sh"

which gcloud
which gsutil
gsutil ver
gsutil ver -l | grep crcmod
gcloud version
cat ~/.boto 2> /dev/null || true
echo '[Credentials]' > ~/.boto
echo "gs_service_key_file = $(get_secret_file GOOGLE_CREDENTIALS_COLLECTOR_SVC_ACCT)" >> ~/.boto
gcloud auth activate-service-account --key-file "$(get_secret_file GOOGLE_CREDENTIALS_COLLECTOR_SVC_ACCT)"
gcloud config set project stackrox-ci
gcloud config set compute/region us-central1
gcloud config set compute/zone us-central1-a
gcloud config set core/disable_prompts True
gcloud auth list
gsutil ls "gs://collector-build-cache" || echo "ERROR: Could not ls bucket"
