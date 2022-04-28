#!/usr/bin/env bash
set -eo pipefail

# /usr/local/lib is not in the library path by default
echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig

mv collector-wrapper.sh /usr/local/bin/
chmod 700 bootstrap.sh
microdnf upgrade -y
microdnf install -y kmod findutils

microdnf clean all
rpm --query --all 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*' 'findutils' | xargs -t rpm -e --nodeps
rm -rf /var/cache/yum

echo "${MODULE_VERSION}" > /kernel-modules/MODULE_VERSION.txt
