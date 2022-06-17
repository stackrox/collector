#!/usr/bin/env python3

import sys
from sh import git, ErrorReturnCode
from semver import VersionInfo
import argparse


class Repository:
    def __init__(self, root='.', dry_run=False):
        self.root = root
        self.dry_run = dry_run
        self.git = git.bake('--no-pager', C=root)

    def reset(self):
        self._run_git('checkout', 'master')
        self._run_git('pull')

    def push_release_tag(self, version: VersionInfo):
        self._run_git('push', 'origin', self._full_version(version))

    def push_release_branch(self, version: VersionInfo):
        self._run_git('push', '--set-upstream', f'release/{self._full_version(version)}')

    def make_release_tag(self, version: VersionInfo, patch: str = None):
        self._run_git('tag', self._full_version(version, patch))

    def checkout_release_branch(self, version: VersionInfo):
        self._run_git('checkout', '-b', f'release/{self._full_version(version)}')
        self._run_git('commit', '--allow-empty', '-m', '"Empty commit"')

    def _full_version(self, base: VersionInfo, patch: str = None) -> str:
        return f'{base.major}.{base.minor}.{patch if patch else "x"}'

    def _run_git(self, command: str, *args):
        print(f'[*] git {command} {" ".join(args)}')
        if self.dry_run:
            return

        try:
            self.git(command, *args)
        except ErrorReturnCode as e:
            print(f'Failed to run "git {command} {" ".join(args)}": {e}')
            sys.exit(1)


def main(version: VersionInfo, dry_run: bool, push: bool):
    repo = Repository(dry_run=dry_run)
    repo.reset()
    repo.make_release_tag(version)
    repo.checkout_release_branch(version)
    repo.make_release_tag(version, patch='0')

    if push:
        repo.push_release_tag()
        repo.push_release_branch()


if __name__ == '__main__':
    description = """Collector Release Management Tool"""
    parser = argparse.ArgumentParser(description=description)

    def version_parser(vers: str) -> VersionInfo:
        return VersionInfo.parse(f'{vers}.0')

    parser.add_argument('version', help='The release version of collector in the format X.Y', type=version_parser)
    parser.add_argument('-d', '--dry-run', help='If set, simulate the release, logging actions that would have been taken.', action='store_true')
    parser.add_argument('-p', '--push', help='If set, push new tags and branches to the remote', action='store_true')

    args = parser.parse_args()

    main(args.version, args.dry_run, args.push)
