#!/usr/bin/env bash

set -eo pipefail

GH_KEY=$1

if [[ -z $GH_KEY ]]; then
  echo "Empty ssh key"
  exit 1
fi

mkdir -p ~/.ssh
echo "$GH_KEY" > ~/.ssh/id_ed25519
chmod 600 ~/.ssh/id_ed25519

# get fingerprint from github
GH_PUBKEY="$(ssh-keyscan github.com 2> /dev/null)"
GH_FINGERPRINT="$(echo ${GH_PUBKEY} | ssh-keygen -lf - | cut -d" " -f2)"

# Verify from: https://help.github.com/en/articles/githubs-ssh-key-fingerprints
GH_FINGERPRINT_VERIFY="SHA256:nThbg6kXUpJWGl7E1IGOCspRomTxdCARLviKw6E5SY8"
[[ "${GH_FINGERPRINT}" == "${GH_FINGERPRINT_VERIFY}" ]] || {
  echo >&2 "Unexpected SSH key fingerprint for github.com : ${GH_FINGERPRINT} != ${GH_FINGERPRINT_VERIFY}"
  exit 1
}

echo "${GH_PUBKEY}" > ~/.ssh/known_hosts
