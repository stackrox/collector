#!/usr/bin/env bash

version=$1
distro=$2
kernel_version=$3
kernel_major=$4
custom_build_flavors_all_file=${5:-~/kobuild-tmp/custom-flavors/all}

compareVersions() {
  kernel_version_other=$1
  kernel_major_other=$2

  if (( kernel_version_other < kernel_version )); then
    echo "later"
  elif (( kernel_version_other == kernel_version)); then
    if (( kernel_major_other < kernel_major )); then
      echo "later"
    elif (( kernel_major_other == kernel_major )); then
      echo "same"
    else
      echo "earlier"
    fi
  else
    echo "earlier"
  fi

}

getFlavorForDebian() {
  debian_flavor="default"
  build_id="$(echo "${version}" | cut -d '-' -f2 | sed 's|\..*||')"
  if [[ "$(compareVersions 5 10)" == "later" ]]; then
    debian_flavor="hirsute"
  elif (( kernel_version == 5 )); then
    debian_flavor="modern"
  elif [[ "$version" =~ ^4.19.0-[0-9]*(-cloud|)-amd64$ ]]; then
    debian_flavor="modern"
  elif (( build_id >= 14 )); then
    debian_flavor="modern"
  fi
  echo $debian_flavor
}

getFlavorFor5_13_plus() {
  if [[ "$distro" == "ubuntu" ]]; then
    flavor_local="impish"
  elif [[ "$version" == "5.13.0-58.fc35.x86_64" ]]; then
    flavor_local="impish"
  elif [[ "$version" =~ ^5.14.0-(60|61).fc ]]; then
    flavor_local="impish"
  elif [[ "$version" =~ ^5.14.[0-9]-300.fc35.x86_64 ]]; then
    flavor_local="impish"
  else
    flavor_local="hirsute"
  fi
  echo $flavor_local
}

if (( $# < 4 )); then
  echo "ERROR: You do not have enough arguments"
  echo "USAGE: $0 version distro kernel_version kernel_major [custom_build_flavors_all_file]"
  exit 2
fi


flavor="default"
# Ubuntu 20.04 backport
if [[ "$distro" == "ubuntu" && "$version" =~ "~20.04" ]]; then
  flavor="modern"
elif (( kernel_version == 5 && kernel_major >= 13 )); then
  flavor="$(getFlavorFor5_13_plus)"
elif [[ "$distro" == "debian" ]] ; then
  flavor="$(getFlavorForDebian)"
elif [[ "$(compareVersions 5 9)" == "later" ]]; then
  flavor="hirsute"
elif (( kernel_version == 5 )); then
  flavor="modern"
elif [[ "$distro" =~ ^suse$|^dockerdesktop$ ]]; then
  flavor="modern"
# RHEL 8.3+ kernels require a newer gcc and can be compiled with modern builder
#  Match kernels of the form 4.18.0-240.1.1.el8_3.x86_64 and 4.18.0-301.1.el8.x86_64
elif [[ "$distro" == "redhat" && "$version" =~ ^.*(el8_[3-9]|[3-9][0-9][0-9][.0-9]+el8)\.x86_64$ ]]; then
  flavor="modern"
elif [[ "$distro" == "redhat" && "$version" == "4.18.0-293.el8.x86_64" ]]; then
  flavor="modern"
elif grep -q "$distro" <$custom_build_flavors_all_file; then
  flavor="$distro"
fi
echo $flavor
