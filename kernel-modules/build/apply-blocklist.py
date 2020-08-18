#!/usr/bin/env python3

import sys
import re

# Strips whitespaces and comments at the end of line, i.e., turns '  foo  # bar' into '  foo' and
# '  # comment line only' into ''.
strip_comment_re = re.compile(r'\s*(?:#.*)?$')

space_re = re.compile(r'\s+')

def pattern_to_re(pat):
    if not pat:
        return ".*"
    if pat[0] == '~':
        return pat[1:]
    parts = pat.split('*')
    return '.*'.join(re.escape(part) for part in parts)

def open_input(filename):
    if filename == '-':
        return sys.stdin
    return open(filename)

def main(blocklist_file, tasks_file='-'):
    blocklist_regexes = []

    with open(blocklist_file, 'r') as blocklist:
        for line in blocklist:
            line = strip_comment_re.sub('', line).strip()
            if not line:
                continue
            parts = space_re.split(line)
            parts = (parts + ["*", "*"])[:3]

            part_res = [pattern_to_re(p) for p in parts]
            blocklist_regexes.append(r'\s+'.join(part_res))

    blocklist_re = re.compile(r'^(?:%s)$' % r'|'.join((r'(?:%s)' % r for r in blocklist_regexes)))

    with open_input(tasks_file) as f:
        for line in f:
            line = strip_comment_re.sub('', line).strip()
            if not line:
                continue
            if not blocklist_re.match(line):
                print(line)


if __name__ == '__main__':
    main(*sys.argv[1:])
