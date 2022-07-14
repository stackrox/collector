#! /usr/bin/env bash

set -eou pipefail

DISTRO="${DISTRO:-}"

if [[ ${DISTRO} == "rhel7" ]]; then
    BASEURL="https://packages.cloud.google.com/yum/repos/cloud-sdk-el7-x86_64"
    PKG_MANAGER="yum"
else
    BASEURL="https://packages.cloud.google.com/yum/repos/cloud-sdk-el8-x86_64"
    PKG_MANAGER="dnf"
fi

tee -a /etc/yum.repos.d/google-cloud-sdk.repo << EOM
[google-cloud-cli]
name=Google Cloud CLI
baseurl=$BASEURL
enabled=1
gpgcheck=1
repo_gpgcheck=0
gpgkey=https://packages.cloud.google.com/yum/doc/yum-key.gpg
       https://packages.cloud.google.com/yum/doc/rpm-package-key.gpg
EOM

eval "$PKG_MANAGER" install -y google-cloud-cli
