#!/usr/bin/env python3

import collections
import os
import re
import sys

probe_info_re = re.compile(r'^\S+ \S+ (\d+) (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (./collector.*.gz)$')

def bucket_archive_list(f, bytes_per_layer):
    probe_info = []

    for line in f:
        probe_info_match = probe_info_re.match(line)
        if probe_info_match == None or len(probe_info_match.groups()) != 3:
            continue

        size_bytes, created_time, name = probe_info_match.groups()
        probe_info.append({"name":name, "created_time":created_time, "size_bytes":int(size_bytes)})

    remaining_bucket_bytes = 0
    result = []
    for info in sorted(probe_info, key=lambda x: x["created_time"]):
        sz = info["size_bytes"]
        if remaining_bucket_bytes - sz < 0:
            # add new bucket
            result.append([])
            remaining_bucket_bytes = bytes_per_layer
        remaining_bucket_bytes = remaining_bucket_bytes - sz
        result[-1].append(info)

    return result

def write_layer_manifests(probe_buckets, output_dir):
    os.makedirs(output_dir, exist_ok=True)

    for idx, probes in enumerate(probe_buckets):
        print("Layer {} has {} probes".format(idx, len(probes)))

        layer_file = os.path.join(output_dir, "layer_{}.txt".format(idx))
        with open(layer_file, "w") as f:
            f.writelines("%s\n" % probe_info["name"] for probe_info in probes)


def main(args):
    if len(args) != 4:
        raise Exception("Usage: %s <probe-archive-list> <bytes-per-layer> <output-dir>" % (args[0]))

    probe_archive_list_file = args[1]
    bytes_per_layer = int(args[2])
    output_dir = args[3]

    with open(probe_archive_list_file) as f:
        probe_buckets = bucket_archive_list(f, bytes_per_layer)

    write_layer_manifests(probe_buckets, output_dir)

if __name__ == '__main__':
    main(sys.argv)
