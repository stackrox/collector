#!/usr/bin/env bash
set -eo pipefail

# Handle the case that glob doesn't match anything
shopt -s nullglob

for f in ~/kobuild-tmp/custom-flavors/versions.*; do
    flavor="$(basename "$f")"
    flavor="${flavor#versions\.}"
    join -1 1 -2 1 -o'1.1,1.2,1.3' <(sort -k 1b,1 < ~/kobuild-tmp/local-build-tasks) <(sort -k 1b,1 < "$f") > ~/kobuild-tmp/local-build-tasks."$flavor"
done
cat ~/kobuild-tmp/local-build-tasks.* ~/kobuild-tmp/local-build-tasks | sort | uniq -u > ~/kobuild-tmp/local-build-tasks.default
rm ~/kobuild-tmp/local-build-tasks
