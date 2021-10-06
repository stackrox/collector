#!/usr/bin/env bash

input=${1:-"TestInput.txt"}
custom_build_flavors_all_file=custom-build-flavors-all
script="../../get-builder-flavor.sh"

found_failure="false"

while read -r line
do
  version="$(echo $line | awk '{print $1}')"
  distro="$(echo $line | awk '{print $2}')"
  expected_builder="$(echo $line | awk '{print $3}')"
  kernel_version="$(echo $version | awk -F "." '{print $1}')"
  major_version="$(echo $version | awk -F "." '{print $2}')"
  builder="$($script $version $distro $kernel_version $major_version $custom_build_flavors_all_file)"

  if [[ $builder != $expected_builder ]]; then
    found_failure="true"
    echo 
    echo "##############"
    echo "ERROR: Expected builder does not match builder"
    echo $line
    echo "expected_builder= $expected_builder"
    echo "builder= $builder"
    echo "version= $version"
    echo "distro= $distro"
    echo "kernel_version= $kernel_version"
    echo "major_version= $major_version"
    echo "##############"
    echo 
  fi
done < "$input"

if [[ "$found_failure" == "true" ]]; then
  echo "Tests did not pass"
  exit 2
else
  echo "Tests passed"
fi
