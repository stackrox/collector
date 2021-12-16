#!/usr/bin/env bash
set -eo pipefail

mkdir -p ~/workspace/go/src/github.com/stackrox/bundles

## Get the bundle for the running machine
gsutil -m cp "gs://stackrox-kernel-bundles/bundle-$(uname -r).tgz" ~/workspace/go/src/github.com/stackrox/bundles
