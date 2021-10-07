#!/usr/bin/env bash 

SCRIPT_DIR=$(dirname "$0")
script="${SCRIPT_DIR}/../../.circleci/get-builder-flavor.sh"

input=${1:-"$SCRIPT_DIR/TestInput.txt"}
custom_build_flavors_all_file="$SCRIPT_DIR"/custom-build-flavors-all
num_failures=0

while read -r line
do
  version="$(echo $line | awk '{print $1}')"
  distro="$(echo $line | awk '{print $2}')"
  expected_builder="$(echo $line | awk '{print $3}')"
  kernel_version="$(echo $version | cut -d. -f1)"
  major_version="$(echo $version | cut -d. -f2)"
  builder="$($script $version $distro $kernel_version $major_version $custom_build_flavors_all_file)"

  if [[ $builder != $expected_builder ]]; then
    num_failures=$(expr $num_failures + 1)
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

if (( num_failures > 0 )); then
  echo "$num_failures tests did not pass"
  exit 2
else
  echo "Tests passed"
fi
