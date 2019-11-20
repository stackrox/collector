#!/usr/bin/env python3

import collections
import os

def parse_version(ver):
    return [int(c) for c in ver.split('.')]

def mod_version_by_rox_version(md_dir):
    result = {}

    for rox_version in os.listdir(md_dir):
        path = os.path.join(md_dir, rox_version)
        if not os.path.isdir(path):
            continue

        mod_ver = readfile(os.path.join(path, 'MODULE_VERSION'))

        result[rox_version] = mod_ver

def get_version_ranges(version_map):
    rox_versions = list(version_map.keys())
    if not rox_versions:
        return

    rox_versions.sort(key=parse_version, reverse=True)

    curr_range = []
    curr_mod_ver = None
    ranges = collections.defaultdict(list)

    for rox_ver in rox_versions:
        mod_ver = version_map.get(rox_ver)
        if mod_ver != curr_mod_ver:
            curr_range = []
            ranges[mod_ver].append(curr_range)
            curr_mod_ver = mod_ver
        curr_range.append(rox_ver)
