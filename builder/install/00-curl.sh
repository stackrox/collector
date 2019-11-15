#!/usr/bin/env bash

set -e

if [ ! -f /etc/redhat-release ]; then
 echo >&2 "Not building curl from source"
 exit 0
fi

wget "https://curl.haxx.se/download/curl-${CURL_VERSION}.tar.gz"
tar -xzf "curl-${CURL_VERSION}.tar.gz"
cd "curl-${CURL_VERSION}"
./configure --with-ssl --disable-threaded-resolver --enable-shared \
  --enable-optimize --disable-curldebug --disable-rt --enable-http --disable-ftp \
  --disable-file --disable-ldap --disable-ldaps --disable-rtsp --disable-telnet \
  --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp \
  --disable-gopher --disable-sspi --disable-ntlm-wb --disable-tls-srp \
  --without-libmetalink --without-librtmp --without-winidn --without-libidn \
  --without-libidn2 --without-nghttp2 --without-libssh2  --without-libpsl \
  --prefix="/usr/local"
make -j "${NPROCS:-2}"
make install
