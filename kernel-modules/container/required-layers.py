#!/usr/bin/env python3

import os
import argparse
import re

probe_name_re = re.compile(r'^collector.*.gz$')


def get_probe_sizes(probe_dir):
    return [
        f.stat().st_size for f in os.scandir(probe_dir) if probe_name_re.match(f.name)
    ]


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('probe-dir', help='The path to the downloaded probes')
    parser.add_argument('--mb-per-layer', '-m', type=int, default=300, help='Number of megabytes per layer')

    args = parser.parse_args()

    bytes_per_layer = args.mb_per_layer * 1024 * 1024

    probe_sizes = get_probe_sizes(args.probe_dir)

    buckets = 1
    remaining_bucket_bytes = bytes_per_layer
    for size in probe_sizes:
        if remaining_bucket_bytes - size < 0:
            buckets += 1
            remaining_bucket_bytes = bytes_per_layer
        remaining_bucket_bytes -= size

    print(buckets)
