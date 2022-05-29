#!/usr/bin/env bash
set -eo pipefail

echo "${GCLOUD_SSH_KEY_PUB}" > "${GCP_SSH_KEY_FILE}.pub"
chmod 0600 "${GCP_SSH_KEY_FILE}.pub"
