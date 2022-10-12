#!/usr/bin/env bash

set -eo pipefail

gcloud compute instances delete "${GCLOUD_INSTANCE}" --zone us-central1-a
