#!/usr/bin/env bash
set -eo pipefail

microdnf upgrade -y --nobest
microdnf install -y elfutils-libelf

microdnf clean all

SUPERFLUOUS_PACKAGES=$(/usr/local/bin/orphaner bash curl elfutils-libelf openssl-libs libuuid libstdc++ libcurl-minimal)

# shellcheck disable=SC2046,SC2086
rpm --verbose -e --nodeps $(rpm -qa ${SUPERFLUOUS_PACKAGES})

rm -rf /var/cache/yum /usr/local/bin/orphaner
