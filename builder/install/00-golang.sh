#!/usr/bin/env bash
set -e

GO_VERSION=1.23.7
ARCH=$(uname -m)
case ${ARCH} in
    x86_64) GO_ARCH=amd64 ;;
    aarch64) GO_ARCH=arm64 ;;
    ppc64le) GO_ARCH=ppc64le ;;
    s390x) GO_ARCH=s390x ;;
esac

curl -fsSL "https://go.dev/dl/go${GO_VERSION}.linux-${GO_ARCH}.tar.gz" | tar -C /usr/local -xz
export PATH="/usr/local/go/bin:${PATH}"
echo "export PATH=/usr/local/go/bin:${PATH}" >> /etc/profile.d/golang.sh
go version
