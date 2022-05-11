#!/usr/bin/env bash
set -eo pipefail

# /usr/local/lib is not in the library path by default
echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig

mv collector-wrapper.sh /usr/local/bin/
chmod 700 bootstrap.sh
dnf upgrade --nobest -y
dnf install -y kmod

if [ "$ADDRESS_SANITIZER" == "true" ]; then
    dnf install -y libasan
fi

dnf clean all
rpm --query --all 'curl' '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*' | xargs rpm -e --nodeps
rm -rf /var/cache/dnf

if [ "$USE_VALGRIND" == "true" ]; then
    ln -s /valgrind-3.17.0/install/bin/valgrind /usr/local/bin/valgrind
fi

echo "${MODULE_VERSION}" > /kernel-modules/MODULE_VERSION.txt

find /valgrind-3.17.0 -type d -empty -delete
