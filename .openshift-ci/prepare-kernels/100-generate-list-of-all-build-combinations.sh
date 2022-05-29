#!/usr/bin/env bash
set -eo pipefail

# all-build-tasks will contain all potentially possible build tasks, i.e., the cross
# product between the set of kernel versions and the set of module versions
echo > ~/kobuild-tmp/all-build-tasks

# redundant-build-tasks will contain all build tasks for which we already have the resulting
# module.
echo > ~/kobuild-tmp/redundant-build-tasks

cd "${WORKSPACE_ROOT}/ko-build"
echo > ~/kobuild-tmp/all-module-versions

for i in module-versions/*/; do
    version="$(basename "$i")"
    echo "$version" >> ~/kobuild-tmp/all-module-versions

    awk -v modver="$version" '{print $1 " " modver " mod"}' < "${SOURCE_ROOT}/kernel-modules/KERNEL_VERSIONS" >> ~/kobuild-tmp/all-build-tasks
    if [[ -d "${i}/bpf" ]]; then
        awk -v modver="$version" '{print $1 " " modver " bpf"}' < "${SOURCE_ROOT}/kernel-modules/KERNEL_VERSIONS" >> ~/kobuild-tmp/all-build-tasks
    fi
    awk -v modver="$version" '{print $1 " " modver " " $2}' < ~/kobuild-tmp/existing-modules-"${version}" >> ~/kobuild-tmp/redundant-build-tasks
done

# blocklisted-build-tasks is populated from the BLOCKLIST file to exclude build tasks which would fail.
"${SOURCE_ROOT}/kernel-modules/build/apply-blocklist.py" "${SOURCE_ROOT}/kernel-modules/BLOCKLIST" \
    ~/kobuild-tmp/all-build-tasks > ~/kobuild-tmp/non-blocklisted-build-tasks

# Create the set of build tasks as the contents of `all-build-tasks` minus the redundant and blocklisted
# build tasks.
cat ~/kobuild-tmp/non-blocklisted-build-tasks \
    ~/kobuild-tmp/redundant-build-tasks ~/kobuild-tmp/redundant-build-tasks \
    | sort | uniq -u > build-tasks
