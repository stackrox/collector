#!/usr/bin/env bash
set -eo pipefail

microdnf upgrade --nobest
microdnf install kmod findutils elfutils-libelf

microdnf clean all
rpm --query --all 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*' 'findutils' | xargs -t rpm -e --nodeps
rm -rf /var/cache/yum
