#!/usr/bin/env bash

version=$1
distro=$2
kernel_version=$3
kernel_major=$4

flavor="default"
if (( kernel_version == 5 && kernel_major >= 13 )); then
  flavor="hirsute" #Testing if these work for bpf
  #flavor="impish"
elif [[ "$distro" == "debian" ]] ; then
  build_id="$(echo "${version}" | cut -d '-' -f2)"
  if (( kernel_version >= 5 )); then
    if (( kernel_version > 5 || (( kernel_version == 5 && kernel_major > 10 )) )); then
      flavor="hirsute"
    else
      flavor="modern"
    fi
  elif [[ "$version" =~ ^4.19.0-[0-9]*(-cloud|)-amd64$ ]]; then
    flavor="modern"
  elif (( build_id >= 14 )); then
    flavor="modern"
  fi
# Ubuntu 20.04 backport
elif [[ "$distro" == "ubuntu" && "$version" =~ "~20.04" ]]; then
  flavor="modern"
# SUSE and all kernels >= 5 can use modern builder
elif [[ "$distro" =~ ^suse$|^dockerdesktop$ ]] || (( kernel_version >= 5 )); then
  if (( kernel_version > 5 || (( kernel_version == 5 && kernel_major > 10 )) || (( "$distro" == "flatcar" && kernel_version == 5 && kernel_major == 10  )) )); then
    flavor="hirsute"
  else
    flavor="modern"
  fi
# RHEL 8.3+ kernels require a newer gcc and can be compiled with modern builder
#  Match kernels of the form 4.18.0-240.1.1.el8_3.x86_64 and 4.18.0-301.1.el8.x86_64
elif [[ "$distro" == "redhat" && "$version" =~ ^.*(el8_[3-9]|[3-9][0-9][0-9][.0-9]+el8)\.x86_64$ ]]; then
  flavor="modern"
elif [[ "$distro" == "redhat" && "$version" == "4.18.0-293.el8.x86_64" ]]; then
  flavor="modern"
elif [[ "$distro" == "debian" && "$version" =~ ^4.19.0-17(-cloud|)-amd64$ ]]; then
  flavor="modern"
elif grep -q "$distro" <~/kobuild-tmp/custom-flavors/all; then
  flavor="$distro"
fi
echo $flavor
