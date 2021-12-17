#!/usr/bin/env bash
set -eo pipefail

if [[ -z "$ROX_CI_IMAGE" ]]; then
  pip3 install --upgrade pip
  pip3 install wheel
  pip3 install -U crcmod google_compute_engine
  pip3 install -U gcloud gsutil
fi
sudo chown circleci:circleci /opt
gcloud components install gsutil -q
gcloud components update -q
