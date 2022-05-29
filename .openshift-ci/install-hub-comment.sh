#!/usr/bin/env bash
set -eo pipefail

wget --quiet https://github.com/joshdk/hub-comment/releases/download/0.1.0-rc6/hub-comment_linux_amd64
sudo install hub-comment_linux_amd64 /usr/bin/hub-comment
