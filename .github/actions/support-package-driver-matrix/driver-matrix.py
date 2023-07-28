#!/usr/bin/env python3

import argparse
import json
import os
import sys
import re

ebpf_re = re.compile(r'collector-ebpf-(\d+\.\d+\.\d+.*)\.o\.gz$')
kmod_re = re.compile(r'collector-(\d+\.\d+\.\d+.*)\.ko\.gz$')
unavailable_re = re.compile(r'\.collector-ebpf-(\d+\.\d+\.\d+.*)\.unavail$')


def update_kernel(kernel_list: dict,
                  kernel_version: str,
                  driver_version: str,
                  driver_type: str,
                  status: str):
    kernel = kernel_list.get(kernel_version)
    if kernel is None:
        kernel_list[kernel_version] = {
            driver_version: {
                driver_type: status,
            }
        }
    else:
        dver = kernel.get(driver_version)
        if dver is None:
            kernel[driver_version] = {
                driver_type: status,
            }
        else:
            dver[driver_type] = status


def process_line(kernels: dict,
                 driver_version: str,
                 available: str,
                 line: str):
    match = ebpf_re.search(line)
    if match:
        update_kernel(kernels, match[1],
                      driver_version, 'ebpf', available)
        return

    match = kmod_re.search(line)
    if match:
        update_kernel(kernels, match[1],
                      driver_version, 'kmod', available)
        return

    match = unavailable_re.search(line)
    if match:
        update_kernel(kernels, match[1],
                      driver_version, 'ebpf', 'unavailable')
        return

    print('Did not match any known drivers')


def main(kernels: dict, driver_version: str, file, available: str):
    for line in sys.stdin:
        line = line.rstrip()
        process_line(kernels, driver_version, available, line)

    if file:
        json.dump(kernels, file)
    else:
        print(json.dumps(kernels))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('version', help='Driver version being updated')
    parser.add_argument('-u', '--update', help='File to be updated')
    parser.add_argument('-d', '--downstream', action='store_true',
                        help='Mark available drivers as "downstream"')
    args = parser.parse_args()

    version = args.version
    available = 'downstream' if args.downstream else 'upstream'
    kernels = {}

    file = None
    if args.update:
        if os.path.isfile(args.update):
            with open(args.update, 'r') as f:
                kernels = json.load(f)

        file = open(args.update, "w")

    main(kernels, version, file, available)
