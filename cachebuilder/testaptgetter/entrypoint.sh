#!/bin/bash
set -e
time /aptinstalls.sh

test -d /home/srbuser || mkdir /home/srbuser # may already be mounted
chown -R srbuser:srbuser /home/srbuser

mv /sudoers /etc

PATH=/usr/lib/ccache:$PATH HOME=/home/srbuser sudo -E -u srbuser /install-grpc-cpp-plugin.sh
echo "FINISHED"
