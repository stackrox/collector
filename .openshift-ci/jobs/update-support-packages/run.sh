#!/usr/bin/env bash
set -eoux pipefail

gcloud_command() {
    gcloud compute ssh --ssh-key-file="$GCP_SSH_KEY_FILE" "$GCLOUD_USER"@"$GCLOUD_INSTANCE" -- -T "$1"
}

export WORKDIR=/go/src/github.com/stackrox/collector

source "$WORKDIR/.openshift-ci/drivers/scripts/lib.sh"

BRANCH="$(get_branch)"

# shellcheck source=SCRIPTDIR/../../drivers/scripts/lib.sh
source "${WORKDIR}/.openshift-ci/drivers/scripts/lib.sh"

# Uncomment before merging
#if ! pr_has_label "test-support-packages"; then
#   if [ "${BRANCH}" != "master" ]; then
#      exit 0
#   fi
#fi

# shellcheck source=SCRIPTDIR/../../jobs/update-support-packages/env.sh
source "${WORKDIR}/.openshift-ci/jobs/update-support-packages/env.sh"

env


gcloud_command "git clone https://github.com/stackrox/collector.git --single-branch --branch=$BRANCH --depth=1"

#gcloud_command "$SOURCE_ROOT/.openshift-ci/jobs/integration-tests/gcloud-init.sh"

gcloud_command "sudo apt install zip -y"
gcloud_command "$SUPPORT_PKG_SRC_ROOT/run-all.sh $SOURCE_ROOT $SUPPORT_PKG_SRC_ROOT"

RELATIVE_PATH="collector/support-packages"
GCLOUD_BUCKET="gs://sr-roxc"
# Will be used in the future
#PUBLIC_RELATIVE_PATH="offline/v1/support-packages"
#PUBLIC_GCLOUD_BUCKET="gs://collector-support-public"

relative_path="$RELATIVE_PATH"

if [[ "$BRANCH" != "master" ]]; then
    relative_path="${relative_path}/.test-${BUILD_ID}"
fi

GCLOUD_TARGET="${GCLOUD_BUCKET}/${relative_path}"

gcloud_command "gsutil -m rsync -r /tmp/support-packages/output $GCLOUD_TARGET"
