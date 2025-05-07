#!/usr/bin/env bash
set -eo pipefail

dnf upgrade -y
dnf install -y libasan libubsan libtsan elfutils-libelf
