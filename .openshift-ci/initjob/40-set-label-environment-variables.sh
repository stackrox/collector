#!/usr/bin/env bash
set -eo pipefail

shared_env=$1

if [[ -f pr-metadata/labels/build-legacy-probes ]]; then
    echo 'export BUILD_LEGACY_PROBES=true' >> "$shared_env"
else
    echo 'export BUILD_LEGACY_PROBES=false' >> "$shared_env"
fi

if [[ -f pr-metadata/labels/build-full-images ]]; then
    echo 'export BUILD_FULL_IMAGES=true' >> "$shared_env"
else
    echo 'export BUILD_FULL_IMAGES=false' >> "$shared_env"
fi

if [[ -f pr-metadata/labels/debug ]]; then
    echo 'export CMAKE_BUILD_TYPE=Debug' >> "$shared_env"
else
    echo 'export CMAKE_BUILD_TYPE=Release' >> "$shared_env"
fi

if [[ -f pr-metadata/labels/run-stackrox-ci ]]; then
    echo 'export RUN_STACKROX_CI=true' >> ~/workspace/shared-env
else
    echo 'export RUN_STACKROX_CI=false' >> ~/workspace/shared-env
fi

# Loops over all possible types of Valgrind tests, checks the labels, and sets environment variables accordingly
# Default is not to use Valgrind, but build with Valgrind if any Valgrind label is present
BUILD_USE_VALGRIND="false"
for test_type in unit-tests integration-tests; do
    CHECK_VALGRIND="false"
    for valgrind_test_type in helgrind valgrind; do
        up_valgrind_test_type=${valgrind_test_type^^}
        up_test_type="${test_type//-/_}"
        up_test_type="${up_test_type^^}"
        if [[ -f "pr-metadata/labels/${valgrind_test_type}-${test_type}" ]]; then
            echo "export ${up_valgrind_test_type}_${up_test_type}=true" >> "$shared_env"
            CHECK_VALGRIND="true"
            BUILD_USE_VALGRIND="true"
        else
            echo "export ${up_valgrind_test_type}_${up_test_type}=false" >> "$shared_env"
        fi
    done
    echo "export CHECK_${up_valgrind_test_type}_${up_test_type}=$CHECK_VALGRIND" >> "$shared_env"
done

echo "export BUILD_USE_VALGRIND=$BUILD_USE_VALGRIND" >> "$shared_env"

if [[ -f pr-metadata/labels/address-sanitizer ]]; then
    echo 'export SANITIZER_TESTS=true' >> "$shared_env"
else
    echo 'export SANITIZER_TESTS=false' >> "$shared_env"
fi
