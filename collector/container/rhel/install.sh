#!/usr/bin/env bash
set -eo pipefail

microdnf upgrade -y
microdnf install -y kmod findutils

microdnf clean all
rpm --query --all 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*' 'findutils' | xargs -t rpm -e --nodeps
rm -rf /var/cache/yum
