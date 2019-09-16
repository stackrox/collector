#!/usr/bin/env bash

GRPC_CPP_PLUGIN="$(which grpc_cpp_plugin)"
if [[ -n "${GRPC_CPP_PLUGIN}" ]]; then
    echo "gRPC cpp plugin installed."
    exit 0
fi

function install_grpc() {
    GRPC_REVISION=v1.14.1
    echo "Installing gRPC"
    rm -rf /tmp/grpc
    mkdir -p /tmp/grpc
    cd /tmp/grpc
    git clone -b $GRPC_REVISION https://github.com/grpc/grpc \
        && cd grpc \
        && git submodule update --init \
        && make \
        && make prefix=/tmp/build install \
        && sudo cp /tmp/build/bin/grpc_cpp_plugin /usr/local/bin/

    rm -rf /tmp/grpc
    rm -rf /tmp/build
    echo "gRPC cpp plugin installed."
}

UNAME_S="$(uname -s)"
if [[ "${UNAME_S}" = "Linux" ]]; then
    sudo apt-get update && sudo apt-get install -y \
    build-essential autoconf git pkg-config \
    automake libtool curl make g++ unzip dh-autoreconf \
    && apt-get clean
    install_grpc
    exit 0
fi

if [[ "${UNAME_S}" = "Darwin" ]]; then
    CHECK_BREW="$(which brew)"
    # install brew
    if [[ -z "${CHECK_BREW}" ]]; then
        echo "Brew tool not found. Installing brew ..."
        mkdir -p /tmp/homebrew \
        && cd /tmp/homebrew \
        && curl -L https://github.com/Homebrew/brew/tarball/master | tar xz --strip 1 -C homebrew
    fi

    # install build dependencies
    echo "Installing gRPC build dependencies using brew"
    brew install autoconf automake libtool shtool gflags

    install_grpc
fi

exit 0

