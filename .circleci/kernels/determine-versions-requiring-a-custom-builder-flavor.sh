#!/usr/bin/env bash
set -eo pipefail

mkdir -p ~/kobuild-tmp/custom-flavors
make --quiet -C "${SOURCE_ROOT}/kernel-modules" print-custom-flavors >~/kobuild-tmp/custom-flavors/all
mkdir ~/kobuild-tmp/meta-inspect
for bundle_file in ~/kobuild-tmp/bundles/bundle-*.tgz; do
  version="$(basename "$bundle_file" | sed -E 's/^bundle-(.*)\.tgz$/\1/')"
  tar -xzf "${bundle_file}" -C ~/kobuild-tmp/meta-inspect ./BUNDLE_DISTRO ./BUNDLE_VERSION ./BUNDLE_MAJOR
  distro="$(< ~/kobuild-tmp/meta-inspect/BUNDLE_DISTRO)"
  kernel_version="$(< ~/kobuild-tmp/meta-inspect/BUNDLE_VERSION)"
  kernel_major="$(< ~/kobuild-tmp/meta-inspect/BUNDLE_MAJOR)"
  flavor="$("${CI_ROOT}/kernels/get-builder-flavor.sh" $version $distro $kernel_version $kernel_major)"
  if [[ "$flavor" != "default" ]]; then
    echo "$version" >>~/kobuild-tmp/custom-flavors/versions."$flavor"
  fi
  echo "Building kernel version $version with $flavor builder. Distro is $distro"
done
