#!/usr/bin/env python3

import collections
import os
import re
import sys

strip_comment_re = re.compile(r'\s*(#.*)?$')
space_re = re.compile(r'\s+')


def parse_released_versions(f):
    result = collections.defaultdict(list)

    for line in f:
        line = strip_comment_re.sub('', line).strip()
        if not line:
            continue

        versions = space_re.split(line)
        if len(versions) < 2:
            continue

        collector_version, rox_version = versions[:2]
        result[collector_version].append(rox_version)

    return result


def write_versions_metadata(version_map, metadata_dir):
    collector_versions_dir = os.path.join(metadata_dir, 'collector-versions')
    os.makedirs(collector_versions_dir, exist_ok=True)

    for collector_version, rox_versions in version_map.items():
        version_dir = os.path.join(collector_versions_dir, collector_version)
        os.makedirs(version_dir, exist_ok=True)

        rox_versions_file = os.path.join(version_dir, 'ROX_VERSIONS')
        with open(rox_versions_file, "w") as f:
            f.writelines("%s\n" % rox_version for rox_version in rox_versions)


def main(args):
    if len(args) != 3:
        raise Exception("Usage: %s <released-versions-file> <metadata-dir>" % (args[0]))

    released_versions_file = args[1]
    metadata_dir = args[2]

    with open(released_versions_file) as f:
        version_map = parse_released_versions(f)

    write_versions_metadata(version_map, metadata_dir)


if __name__ == '__main__':
    main(sys.argv)
