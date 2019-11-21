#!/usr/bin/env python3

import collections
from collections import namedtuple
from datetime import datetime
import os
import re
import sys
from pathlib import Path
import jinja2

VersionRange = namedtuple('VersionRange', 'min max')

class SupportPackage(object):
    def __init__(self, module_version, rox_version_ranges, file_name, last_update_time):
        self.module_version = module_version
        self.rox_version_ranges = rox_version_ranges
        self.file_name = file_name
        self.last_update_time = last_update_time

    @property
    def download_url(self):
        return '%s/%s/%s' % (os.getenv('BASE_URL'), self.module_version, self.file_name)

    def __repr__(self):
        return 'SupportPackage(module_version=%s, rox_version_ranges=%s, file_name=%s, last_update_time=%d)' % (
            self.module_version, self.rox_version_ranges, self.file_name, self.last_update_time,
        )

def render_index(packages, out_dir, template_file='index.html'):
    curr_dir = Path(__file__).parent.absolute()
    template_loader = jinja2.FileSystemLoader(searchpath=os.path.join(curr_dir, 'templates'))
    template_env = jinja2.Environment(loader=template_loader)
    template = template_env.get_template(template_file)

    output = template.render({'support_packages': packages})

    output_file = os.path.join(out_dir, template_file)
    with open(output_file, "w") as f:
        f.write(output)


def load_support_packages(output_dir, mod_md_map):
    support_packages = []

    for mod_ver in os.listdir(output_dir):
        mod_out_dir = os.path.join(output_dir, mod_ver)
        if not os.path.isdir(mod_out_dir):
            continue

        rox_version_ranges = sorted(mod_md_map[mod_ver], key=lambda r: r.max, reverse=True)

        support_pkg_file_re = re.compile(r'^support-pkg-' + re.escape(mod_ver[:6]) + r'-\d+\.zip$')
        support_pkg_file = max(f for f in os.listdir(mod_out_dir) if support_pkg_file_re.match(f))

        st = os.stat(os.path.join(mod_out_dir, support_pkg_file))
        last_mod_time = datetime.utcfromtimestamp(st.st_mtime).strftime('%Y/%m/%d, %H:%M:%S')

        support_packages.append(SupportPackage(mod_ver, rox_version_ranges, support_pkg_file, last_mod_time))

    support_packages.sort(key=lambda p: p.rox_version_ranges[0].max, reverse=True)
    return support_packages


def parse_version(ver):
    return [int(c) for c in ver.split('.')]

def load_modules_metadata(md_dir):
    result = {}

    mod_vers_dir = os.path.join(md_dir, 'module-versions')
    for mod_ver in os.listdir(mod_vers_dir):
        path = os.path.join(mod_vers_dir, mod_ver)
        if not os.path.isdir(path):
            continue

        with open(os.path.join(path, 'ROX_VERSIONS')) as f:
            rox_versions = [ver for ver in (line.strip() for line in f) if ver]

        result[mod_ver] = rox_versions

    return result

def compute_version_ranges(mod_ver_to_rox_vers):
    rox_ver_to_mod_ver = {}

    for mod_ver, rox_vers in mod_ver_to_rox_vers.items():
        for rox_ver in rox_vers:
            rox_ver_to_mod_ver[rox_ver] = mod_ver

    rox_versions = list(rox_ver_to_mod_ver.keys())
    rox_versions.sort(key=parse_version)

    curr_range = []
    curr_mod_ver = None
    ranges = collections.defaultdict(list)

    for rox_ver in rox_versions:
        mod_ver = rox_ver_to_mod_ver.get(rox_ver)
        if mod_ver != curr_mod_ver:
            curr_range = []
            # Append in the beginning to ensure version ranges appear in reverse chronological order.
            ranges[mod_ver].insert(0, curr_range)
            curr_mod_ver = mod_ver
        curr_range.append(rox_ver)

    return {
        mod_ver: [VersionRange(rox_ver_list[0], rox_ver_list[-1]) for rox_ver_list in rox_ver_lists]
            for mod_ver, rox_ver_lists in ranges.items()
    }

def main(args):
    if len(args) != 3:
        raise Exception("Usage: %s <metadata dir> <output dir>" % args[0])

    md_dir, out_dir = args[1:]

    modules_md = load_modules_metadata(md_dir)
    ranges = compute_version_ranges(modules_md)

    support_packages = load_support_packages(out_dir, ranges)
    render_index(support_packages, out_dir)

if __name__ == '__main__':
    main(sys.argv)
