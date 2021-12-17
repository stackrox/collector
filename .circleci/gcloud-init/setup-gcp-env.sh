#!/usr/bin/env bash
set -eo pipefail

service_account_env=$1
bucket_permission_check=$2

which gcloud
which gsutil
gsutil ver
gsutil ver -l | grep crcmod
gcloud version
cat ~/.boto || true
echo '[Credentials]' >~/.boto
echo 'gs_service_key_file = /tmp/gcp.json' >>~/.boto
echo "$service_account_env" > /tmp/gcp.json
gcloud auth activate-service-account --key-file /tmp/gcp.json
gcloud config set project stackrox-ci
gcloud config set compute/region us-central1
gcloud config unset compute/zone
gcloud config set core/disable_prompts True
gcloud auth list
gsutil ls $bucket_permission_check/ || echo "ERROR: Could not ls bucket"
