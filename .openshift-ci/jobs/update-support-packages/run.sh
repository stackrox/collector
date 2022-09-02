#!/usr/bin/env bash
set -eou pipefail

gcloud_command() {
    gcloud compute ssh --ssh-key-file="$GCP_SSH_KEY_FILE" "$GCLOUD_USER"@"$GCLOUD_INSTANCE" -- -T "$1"
}

sleep_echo() {
    for _ in {1..120}; do
        sleep 1
        echo -n .
    done
    echo
}

export WORKDIR=/go/src/github.com/stackrox/collector

source "$WORKDIR/.openshift-ci/drivers/scripts/lib.sh"

BRANCH="$(get_branch)"

# shellcheck source=SCRIPTDIR/../../drivers/scripts/lib.sh
source "${WORKDIR}/.openshift-ci/drivers/scripts/lib.sh"

if ! pr_has_label "test-support-packages"; then
    if [ "${BRANCH}" != "master" ]; then
        exit 0
    fi
fi

# shellcheck source=SCRIPTDIR/../../jobs/update-support-packages/env.sh
source "${WORKDIR}/.openshift-ci/jobs/update-support-packages/env.sh"

env

gcloud_command "git clone https://github.com/stackrox/collector.git"
gcloud_command "cd $SOURCE_ROOT; git checkout $BRANCH"

gcloud_command "sudo apt install zip -y"
gcloud_command "$SUPPORT_PKG_SRC_ROOT/run-all.sh $SOURCE_ROOT $SUPPORT_PKG_SRC_ROOT"

RELATIVE_PATH="collector/support-packages"
GCLOUD_BUCKET="gs://sr-roxc"
PUBLIC_GCLOUD_BUCKET="gs://collector-support-public"
PUBLIC_RELATIVE_PATH="offline/v1/support-packages"
PUBLIC_GCLOUD_TARGET="${PUBLIC_GCLOUD_BUCKET}/${PUBLIC_RELATIVE_PATH}"

relative_path="$RELATIVE_PATH"

if [[ "$BRANCH" != "master" ]]; then
    relative_path="${relative_path}/.test-${JOB_ID}"
fi

GCLOUD_TARGET="${GCLOUD_BUCKET}/${relative_path}"

echo "GCLOUD_TARGET= $GCLOUD_TARGET"
gcloud_command "gsutil -m rsync -r /tmp/support-packages/output $GCLOUD_TARGET"
sleep_echo

[[ "$GCLOUD_TARGET" =~ ^gs://[^/]+/.*collector.*/.*support-packages.*$ ]]

[[ "$BRANCH" == "master" || "$GCLOUD_TARGET" = *.test* ]]
gcloud_command "gsutil -m rsync -n -r -d /tmp/support-packages/output $GCLOUD_TARGET"

if [[ "$BRANCH" != "master" ]]; then
    exit 0
fi

echo "PUBLIC_GCLOUD_TARGET= $PUBLIC_GCLOUD_TARGET"
gcloud_command "gsutil -m rsync -r /tmp/support-packages/output $PUBLIC_GCLOUD_TARGET"
sleep_echo
gcloud_command "gsutil -m rsync -n -r -d /tmp/support-packages/output $PUBLIC_GCLOUD_TARGET"
