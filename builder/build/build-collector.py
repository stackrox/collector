#!/usr/bin/env python3

import argparse
import shutil
import multiprocessing
from subprocess import check_call
import os
import re

g_num_procs = multiprocessing.cpu_count()


def fix_includes(path: str):
    fixed = []
    matcher = re.compile(r"#include <grpc/impl/codegen/port_platform.h>")
    replacement_lines = [
        "#include <grpc/impl/codegen/port_platform.h>\n",
        "#ifdef GRPC_ASAN_ENABLED\n"
        "#  undef GRPC_ASAN_ENABLED\n",
        "#endif\n"
    ]
    with open(path, 'r') as in_file:
        for line in in_file:
            if matcher.match(line):
                fixed.extend(replacement_lines)
            else:
                fixed.append(line)

    with open(path, 'w') as out_file:
        out_file.write("".join(fixed))


def file_contains(path: str, needle: str) -> bool:
    with open(path) as f:
        return needle in f.read()


if __name__ == '__main__':
    parser = argparser.ArgumentParser(description='Collector Builder')
    parser.add_argument('--build-type', choices=('Release', 'Debug'),
                        default='Release')
    parser.add_argument('--use-address-sanitizer', action='store_true',
                        help='Whether to use the address sanitiser in the cmake build')
    parser.add_argument('--append-cid', action='store_true',
                        help='Whether to append the CID')

    args = parser.parse_args()

    if args.use_address_sanitizer:
        # Needed for address sanitizer to work. See https://github.com/grpc/grpc/issues/22238.
        # When Collector is built with address sanitizer it sets GRPC_ASAN_ENABLED, which changes a struct in the grpc library.
        # If grpc is compiled without that flag and is then linked with Collector the struct will have
        # two different definitions and Collector will crash when trying to connect to a grpc server.
        for root, _, fnames in os.walk("/src/generated"):
            for filename in fnames:
                if not filename.endswith(".h"):
                    continue
                full_path = os.path.join(root, filename)
                if file_contains(full_path, "port_platform.h"):
                    fix_includes(full_path)

    cmake_cmd = [
        "cmake",
        f"-DCMAKE_BUILD_TYPE={args.build_type.title()}",
        f"-DADDRESS_SANITIZER={args.use_address_sanitizer}",
        f"-DCOLLECTOR_APPEND_CID={args.append_cid}",
        "/src"
    ]

    check_call(args=cmake_cmd, shell=True)
    check_call(args=["make", f"-j{g_num_procs}", "all"], shell=True)

    if parser.build_type == "Release":
        print("strip unneeded")
        check_call(args=[
            "strip", "--strip-unneeded",
            "./collector",
            "./EXCLUDE_FROM_DEFAULT_BUILD/libsinsp/libsinsp-wrapper.so"
        ])

    shutil.copytree("/THIRD_PARTY_NOTICES", ".")
