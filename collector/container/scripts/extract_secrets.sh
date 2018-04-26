#!/bin/sh

set -x

(cd /run/secrets; tar x)

exec /bootstrap.sh "$@"
