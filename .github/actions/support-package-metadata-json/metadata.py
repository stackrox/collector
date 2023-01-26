#!/usr/bin/env python3

import json
import os
from datetime import datetime


def main():
    support_pkg_dir = os.environ['SUPPORT_PACKAGE_TMP_DIR']

    for mod_ver in os.listdir(support_pkg_dir):
        version_dir = os.path.join(support_pkg_dir, mod_ver)

        with open(os.path.join(version_dir, 'latest'), 'r') as f:
            latest_file = f.readline()

            st = os.stat(os.path.join(version_dir, latest_file))
            last_mod_time = datetime.utcfromtimestamp(st.st_mtime).strftime('%Y/%m/%d, %H:%M:%S')

            metadata = {
                'file_name': latest_file,
                'last_modified': last_mod_time,
            }

            with open(os.path.join(version_dir, 'metadata.json'), 'w') as output:
                json.dump(metadata, output)


if __name__ == '__main__':
    main()
