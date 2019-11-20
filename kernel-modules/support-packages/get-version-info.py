#!/usr/bin/env python3

import re

strip_comment_re = re.compile(r'\s*(#.*)?$')
space_re = re.compile(r'\s+')

def parse_released_versions(f):
    result = {}
    for line in f:
        line = strip_comment_re.sub('', line).strip()
        if not line:
            continue

        versions = space_re.split(line)
        if len(versions) < 2:
            continue

        collector_version, rox_version = versions[:2]
        result[rox_version] = collector_version

    return result

