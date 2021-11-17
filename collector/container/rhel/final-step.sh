#!/usr/bin/env bash
set -eo pipefail

mv collector-wrapper.sh /usr/local/bin/
chmod 700 bootstrap.sh
dnf upgrade -y
dnf install -y kmod

if [ "$ADDRESS_SANITIZER" == "true" ]; then
  dnf install -y libasan
fi

dnf clean all
rpm -e --nodeps $(rpm -qa '*rpm*' '*dnf*' '*libsolv*' '*hawkey*' 'yum*')
rm -rf /var/cache/dnf

if [ "$USE_VALGRIND" == "true" ]; then
  ln -s /valgrind-3.17.0/install/bin/valgrind /usr/local/bin/valgrind
fi

echo "${MODULE_VERSION}" > /kernel-modules/MODULE_VERSION.txt

find /valgrind-3.17.0 -type d -empty -delete
