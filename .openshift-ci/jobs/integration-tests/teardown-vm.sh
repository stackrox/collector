#!/usr/bin/env bash

set -exo pipefail

gcloud compute instances delete "collector-osci-${VM_TYPE}-tests-${PROW_JOB_ID:0:8}"
