#!/usr/bin/env python3

# Utility script for dividing kernel probes into multiple layers so that the
# image can be properly cached by CloudFlare. Given a directory that contains
# kernel probes and max MB per layer, output a a manifest file for each layer.

import os
import re
import sys

probe_name_re = re.compile(r'^collector.*.gz$')


def buckets_from_probe_dir(probe_dir, bytes_per_layer, verbose=False):
    probe_info = []
    print(f"Scanning {probe_dir}")
    for f in os.scandir(probe_dir):
        print(f"Matching {f.name}")
        if probe_name_re.match(f.name):
            print("Match")
            probe_info.append({
                "name": f.name,
                "mtime": f.stat().st_mtime,
                "size": f.stat().st_size
            })

    remaining_bucket_bytes = 0
    result = []
    for info in sorted(probe_info, key=lambda x: x["mtime"]):
        sz = info["size"]
        if remaining_bucket_bytes - sz < 0:
            result.append([])
            remaining_bucket_bytes = bytes_per_layer
        remaining_bucket_bytes = remaining_bucket_bytes - sz
        result[-1].append(info)

    return result


def write_layer_manifests(probe_buckets, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    for idx, probes in enumerate(probe_buckets):
        layer_file = os.path.join(output_dir, "{}".format(idx))
        with open(layer_file, "w") as f:
            f.writelines("%s\n" % probe_info["name"] for probe_info in probes)


def main(args):
    if len(args) != 6:
        raise Exception("Usage: %s <max-depth> <megabytes-per-layer> <probe-dir> <output-dir> <verbose>" % (args[0]))
    max_layer_depth = int(args[1])
    bytes_per_layer = int(args[2]) * 1024 * 1024
    probe_dir = args[3]
    output_dir = args[4]
    verbose = bool(args[5])

    probe_buckets = buckets_from_probe_dir(probe_dir, bytes_per_layer, verbose)

    if max_layer_depth > 0 and len(probe_buckets) > max_layer_depth:
        raise Exception("Need %d layers, but configured with max layer depth %d" % (len(probe_buckets), max_layer_depth))

    if verbose:
        print(f"Probe buckets {probe_buckets}")
    else:
        print(len(probe_buckets))

    if output_dir != "-":
        write_layer_manifests(probe_buckets, output_dir)


if __name__ == '__main__':
    main(sys.argv)
