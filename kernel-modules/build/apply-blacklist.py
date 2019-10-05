#!/usr/bin/env python3

import sys
import re

strip_comment_re = re.compile(r'\s*(?:#.*)?$')
space_re = re.compile(r'\s+')

def pattern_to_re(pat):
    if not pat:
        return ".*"
    if pat[0] == '~':
        return pat[1:]
    return pat.replace('*', '.*')

def open_input(filename):
    if filename == '-':
        return sys.stdin
    return open(filename)

def main(blacklist_file, tasks_file='-'):
    blacklist_regexes = []

    with open(blacklist_file, 'r') as blacklist:
        for line in blacklist:
            line = strip_comment_re.sub('', line).strip()
            if not line:
                continue
            parts = space_re.split(line)
            parts = (parts + ["*", "*"])[:3]

            part_res = [pattern_to_re(p) for p in parts]
            blacklist_regexes.append(r'\s+'.join(part_res))

    blacklist_re = re.compile(r'^(?:%s)$' % r'|'.join((r'(?:%s)' % r for r in blacklist_regexes)))

    with open_input(tasks_file) as f:
        for line in f:
            line = strip_comment_re.sub('', line).strip()
            if not line:
                continue
            if not blacklist_re.match(line):
                print(line)


if __name__ == '__main__':
    main(*sys.argv[1:])
