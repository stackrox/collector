#!/usr/bin/env bash
set -eo pipefail

which gcloud
which gsutil
gsutil ver
gsutil ver -l | grep crcmod
gcloud version
cat ~/.boto || true
echo '[Credentials]' > ~/.boto
echo 'gs_service_key_file = /tmp/secret/stackrox-collector-kernel-packer-crawl/GOOGLE_CREDENTIALS_KERNEL_CACHE' >> ~/.boto
gcloud auth activate-service-account --key-file /tmp/secret/stackrox-collector-kernel-packer-crawl/GOOGLE_CREDENTIALS_KERNEL_CACHE
gcloud config set project stackrox-ci
gcloud config set compute/region us-central1
gcloud config unset compute/zone
gcloud config set core/disable_prompts True
gcloud auth list
gsutil ls "gs://collector-build-cache" || echo "ERROR: Could not ls bucket"
