#!/usr/bin/env bash
set -eo pipefail

# UBI 9 requires confirmation with -y flag.
microdnf upgrade -y --nobest
microdnf install -y kmod findutils elfutils-libelf gdb procps

microdnf clean all
#rpm --query --all 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*' 'findutils' | xargs -t rpm -e --nodeps
rm -rf /var/cache/yum
