#!/usr/bin/env python3

# Create a matrix listing where every driver was built.
#
# The resulting json object looks something like this:
# {
#   "3.10.0-123.1.2.el7.x86_64": {
#     "2.2.0": {
#       "ebpf": "unavailable",
#       "kmod": "upstream"
#     },
#     "2.3.0": {
#       "ebpf": "unavailable",
#       "kmod": "upstream"
#     },
#     "2.4.0": {
#       "ebpf": "unavailable",
#       "kmod": "upstream"
#     },
#     "2.5.0": {
#       "ebpf": "unavailable"
#     }
#   },
#   "4.18.0-80.el8.x86_64": {
#     "2.2.0": {
#       "kmod": "upstream",
#       "ebpf": "upstream"
#     },
#     "2.3.0": {
#       "kmod": "upstream",
#       "ebpf": "upstream"
#     },
#     "2.4.0": {
#       "kmod": "upstream",
#       "ebpf": "upstream"
#     },
#     "2.5.0": {
#       "ebpf": "upstream"
#     }
#   },
#   ...
# }
#
# If you are interested on finding a specific kernel, you can use jq with a
# filter similar to the following:
#   jq '."6.2.8-300.fc38.x86_64"' driver-matrix.json
#   {
#     "2.4.0": {
#       "kmod": "upstream",
#       "ebpf": "upstream"
#     },
#     "2.5.0": {
#       "ebpf": "upstream"
#     }
#   }

import argparse
import json
import os
import sys
import re

ebpf_re = re.compile(r'(\d+\.\d+\.\d+)/collector-ebpf-(\d+\.\d+\.\d+.*)\.o\.gz$')
kmod_re = re.compile(r'(\d+\.\d+\.\d+)/collector-(\d+\.\d+\.\d+.*)\.ko\.gz$')
unavailable_re = re.compile(r'(\d+\.\d+\.\d+)/\.collector-ebpf-(\d+\.\d+\.\d+.*)\.unavail$')


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
                 available: str,
                 line: str):
    match = ebpf_re.search(line)
    if match:
        driver_version = match[1]
        kernel_version = match[2]
        update_kernel(kernels, kernel_version,
                      driver_version, 'ebpf', available)
        return

    match = kmod_re.search(line)
    if match:
        driver_version = match[1]
        kernel_version = match[2]
        update_kernel(kernels, kernel_version,
                      driver_version, 'kmod', available)
        return

    match = unavailable_re.search(line)
    if match:
        driver_version = match[1]
        kernel_version = match[2]
        update_kernel(kernels, kernel_version,
                      driver_version, 'ebpf', 'unavailable')
        return

    print('Did not match any known drivers')


def main(kernels: dict, file, available: str):
    for line in sys.stdin:
        line = line.rstrip()
        process_line(kernels, available, line)

    if file:
        json.dump(kernels, file)
    else:
        print(json.dumps(kernels))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-u', '--update', help='File to be updated')
    parser.add_argument('-d', '--downstream', action='store_true',
                        help='Mark available drivers as "downstream"')
    args = parser.parse_args()

    available = 'downstream' if args.downstream else 'upstream'
    kernels = {}

    file = None
    if args.update:
        if os.path.isfile(args.update):
            with open(args.update, 'r') as f:
                kernels = json.load(f)

        file = open(args.update, "w")

    main(kernels, file, available)
