#! /usr/bin/env python3
import os
import re

unavailable_re = re.compile(r'^\.collector-(?:ebpf-)?(.*)\.unavail$')
ebpf_re = re.compile(r'^collector-ebpf-(.*)\.o\.gz$')
kmod_re = re.compile(r'^collector-(.*)\.ko\.gz$')
base_dir = os.environ.get('BASE_DIR', '')
kernels_file = os.environ.get('KERNELS_FILE', '/kernels/all')


def get_kernel_version(driver):
    match = ebpf_re.match(driver)
    if match:
        return match[1]

    match = kmod_re.match(driver)
    if match:
        return match[1]

    match = unavailable_re.match(driver)
    if match:
        return match[1]

    return None


def main(kernels):
    for root, _, drivers in os.walk(f'{base_dir}/kernel-modules'):
        for driver in drivers:
            kernel_version = get_kernel_version(driver)
            driver_path = os.path.join(root, driver)

            if not kernel_version:
                print(f'Invalid driver {driver}')
                os.unlink(driver_path)

            if kernel_version not in kernels:
                print(f'Removing {driver_path}')
                os.unlink(driver_path)


if __name__ == '__main__':
    with open(kernels_file, 'r') as f:
        kernels = f.read().split()

    main(kernels)
