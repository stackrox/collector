#!/usr/bin/env bash
set -eo pipefail

microdnf upgrade -y --nobest
microdnf install -y elfutils-libelf

microdnf clean all
# shellcheck disable=SC2046
rpm --verbose -e --nodeps $(
    rpm -qa '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*' 'libyaml*' 'libarchive*'
)
rm -rf /var/cache/yum
