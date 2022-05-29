#!/usr/bin/env bash
set -eo pipefail

SOURCE_DIR=$1
SCRIPT_DIR="${SOURCE_DIR}/.circleci/test-scripts/integration-test"

input=${2:-"$SCRIPT_DIR/TestInput.txt"}
script="${SOURCE_DIR}/.circleci/integration-test/10-check-whether-to-run-job.sh"

num_failures=0

while read -r -a line; do
    dockerized="${line[0]}"
    image_family="${line[1]}"
    trigger_source="${line[2]}"
    BRANCH="${line[3]}"
    TAG="${line[4]}"
    expected_should_run="${line[5]}"

    if [[ "$TAG" == "unset" ]]; then
        unset TAG
    fi

    if "$script" "$dockerized" "$image_family" "$trigger_source" "$BRANCH" "$TAG"; then
        should_run="false"
    else
        should_run="true"
    fi

    if [[ "$should_run" != "$expected_should_run" ]]; then
        num_failures=$((num_failures + 1))
        echo
        echo "##############"
        echo "ERROR: Expected should_run does not match should_run"
        echo "expected_should_run= $expected_should_run"
        echo "should_run= $should_run"
        echo "dockerized= $dockerized"
        echo "image_family= $image_family"
        echo "trigger_source= $trigger_source"
        echo "TAG= $TAG"
        echo "##############"
        echo
    fi
done < "$input"

if ((num_failures > 0)); then
    echo "$num_failures tests did not pass"
    exit 2
else
    echo "Tests passed"
fi
