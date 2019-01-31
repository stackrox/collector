#!/bin/bash
set -e
date
apt update -y
apt install -y \
apt-utils \
autoconf \
automake \
clang \
cmake \
curl \
g++ \
gcc \
git \
jq \
libc-ares-dev \
libelf-dev \
libgoogle-perftools-dev \
libssl1.0-dev \
libtool \
llvm \
make \
python-pip \
python3-pip \
sudo \
tmux \
unzip \
vim \
wget \
zlib1g-dev

# ccache must be last so symlinks are fully created
apt install -y ccache

echo "apt installs completed successfully"
exit 0
