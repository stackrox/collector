#!/bin/bash
set -e
ls -lRh /builder
time /aptinstalls.sh
docker ps

test -d /home/srbuser || mkdir /home/srbuser # may already be mounted
chown -R srbuser:srbuser /home/srbuser

mv /sudoers /etc

PATH=/usr/lib/ccache:$PATH HOME=/home/srbuser sudo -E -u srbuser /install-grpc-cpp-plugin.sh
PATH=/usr/lib/ccache:$PATH HOME=/home/srbuser sudo -E -u srbuser /builder/install/00-googletest.sh
docker images
echo "FINISHED"
