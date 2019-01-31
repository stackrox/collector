#!/bin/bash
set -e
date
apt update -y
apt install -y \
apt-utils \
autoconf \
automake \
binutils \
build-essential \
ca-certificates \
clang \
cmake \
curl \
g++ \
gcc \
git \
jq \
libcap-ng-dev \
libc-ares-dev \
libcurl4-openssl-dev \
libelf-dev \
libgoogle-perftools-dev \
libluajit-5.1 \
libssl1.0-dev \
libtool \
libz-dev \
llvm \
make \
python \
python3-pip \
python-pip \
sudo \
tmux \
unzip \
uuid-dev \
vim \
wget \
zlib1g-dev

# ccache must be last so symlinks are fully created
apt install -y ccache

echo "apt installs completed successfully"
exit 0
