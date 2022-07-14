#!/usr/bin/env bash
set -eo pipefail

dnf update -y
dnf install epel-release -y

tee -a /etc/yum.repos.d/google-cloud-sdk.repo << EOM
[google-cloud-sdk]
name=Google Cloud SDK
baseurl=https://packages.cloud.google.com/yum/repos/cloud-sdk-el7-x86_64
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=https://packages.cloud.google.com/yum/doc/yum-key.gpg
       https://packages.cloud.google.com/yum/doc/rpm-package-key.gpg
EOM

cat -A /etc/yum.repos.d/google-cloud-sdk.repo

dnf install google-cloud-sdk -y
