#!/usr/bin/env bash
set -eo pipefail

dnf upgrade -y
dnf install -y kmod libasan elfutils-libelf
