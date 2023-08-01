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

kernel_version_re = re.compile(r'(\d+\.\d+\.\d+.*)\.(o|ko)\.gz$')
unavailable_re = re.compile(r'\.collector-ebpf-(\d+\.\d+\.\d+.*)\.unavail$')


def update_kernel(kernel_list: dict,
                  kernel_version: str,
                  driver_version: str,
                  driver_type: str,
                  source: str):
    kernel = kernel_list.get(kernel_version)
    if kernel is None:
        kernel_list[kernel_version] = {
            driver_version: {
                driver_type: source,
            }
        }
    else:
        dver = kernel.get(driver_version)
        if dver is None:
            kernel[driver_version] = {
                driver_type: source,
            }
        else:
            dver[driver_type] = source


def process_line(kernels: dict,
                 source: str,
                 line: str):
    (driver_path, driver) = os.path.split(line)
    (_, driver_version) = os.path.split(driver_path)

    match = kernel_version_re.search(driver)
    if match:
        kernel_version = match[1]
        driver_type = 'kmod' if match[2] == 'ko' else 'ebpf'
        update_kernel(kernels, kernel_version,
                      driver_version, driver_type, source)
        return

    match = unavailable_re.search(driver)
    if match:
        kernel_version = match[1]
        update_kernel(kernels, kernel_version,
                      driver_version, 'ebpf', 'unavailable')
        return

    print('Did not match any known drivers')


def main(file: str, source: str):
    kernels = {}

    if file is None:
        file = sys.stdout
    else:
        if os.path.isfile(file):
            with open(file, 'r') as f:
                kernels = json.load(f)

        file = open(file, "w")

    for line in sys.stdin:
        process_line(kernels, source, line.rstrip())

    json.dump(kernels, file)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-u', '--update', help='File to be updated')
    parser.add_argument('-d', '--downstream', action='store_true',
                        help='Mark available drivers as "downstream"')
    args = parser.parse_args()

    source = 'downstream' if args.downstream else 'upstream'
    file = args.update

    main(file, source)
