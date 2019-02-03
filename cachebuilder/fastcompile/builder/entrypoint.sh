#!/bin/bash
set -e
ls -lRh /builder
ls -lhF /
time /builder/build/aptinstalls.sh
docker ps

test -d /home/srbuser || mkdir /home/srbuser # may already be mounted
chown -R srbuser:srbuser /home/srbuser

mv /builder/build/sudoers /etc

PATH=/usr/lib/ccache:$PATH HOME=/home/srbuser sudo -E -u srbuser /builder/build/install-grpc-cpp-plugin.sh
for i in /builder/install/* ; do
	echo "Executing $i ..."
	PATH=/usr/lib/ccache:$PATH HOME=/home/srbuser sudo -E -u srbuser $i
	echo "Succeeded $i ."
done
sudo chmod a+rwX -R /builder/sysdig/src
PATH=/usr/lib/ccache:$PATH HOME=/home/srbuser sudo -E -u srbuser /builder/sysdig/build/build.sh
cp -a /usr/lib/x86_64-linux-gnu/ /sysdig-build/ul-x86_64-linux-gnu/
cp -a     /lib/x86_64-linux-gnu/ /sysdig-build/l-x86_64-linux-gnu/
docker images
echo "FINISHED"
