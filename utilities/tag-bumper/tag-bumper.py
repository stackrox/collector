#! /usr/bin/env python3

from sh import git, ErrorReturnCode
import argparse
import sys
import os
import atexit
import re


def exit_handler(repo):
    """
    Rollback the repo to the branch passed as an argument.

    Parameters:
        repo: An sh.Command baked for git on the working repository.
    """
    print('Rolling back to starting branch')
    repo.checkout('-')


def validate_version(version: str):
    """
    Validates the provided version is in the form 'M.m'.

    Returns:
        The same string provided as input if the format is valid.

    Raises:
        ValueError If the provided version does not match the expected pattern.
    """
    version_re = re.compile(r'(:?^\d+\.\d+$)')

    if not version_re.match(version):
        raise ValueError

    return version


def get_repo_handle(path: str):
    """
    Provides a sh.Command baked to run git commands on a repository.

    Parameters:
        path: A path to a repository, if it is empty, the returned handle points to the directory this script lives in.

    Returns:
        An sh.Command ready to run git commands.
    """
    if path != '':
        return git.bake('--no-pager', C=path)
    return git.bake('--no-pager', C=os.path.dirname(os.path.realpath(__file__)))


def get_release_branch(version: str) -> str:
    """
    Helper function, simply formats the release branch for the provided version.

    Parameters:
        version: A string with a valid version.

    Returns:
        A string with the name of the corresponding release branch.
    """
    return f'release/{version}.x'


def fetch_all(repo):
    """
    Fetches all branches and tags from all remotes configured in the repository.

    Parameters:
        repo: An sh.Command baked for git on the working repository.
    """
    try:
        repo.fetch('--all', '--tags')
    except ErrorReturnCode as e:
        print(f'Failed to fetch remote. {e}')
        sys.exit(1)


def get_branch(repo, version: str) -> str:
    """
    Validates the release branch exists and returns a string with its name.

    Parameters:
        repo: An sh.Command baked for git on the working repository.
        version: A string with a valid version.

    Returns:
        A string with the name of the release branch.
    """
    release_branch = get_release_branch(version)

    try:
        repo('rev-parse', '--verify', release_branch)
    except ErrorReturnCode as e:
        print(f'The branch {release_branch} does not exist. {e}')
        sys.exit(1)

    return release_branch


def checkout_release_branch(repo, version: str):
    """
    Checks out the release branch for the provided version.

    Parameters:
        repo: An sh.Command baked for git on the working repository.
        version: A string with a valid version.
    """
    branch = get_branch(repo, version)
    print(f'Checking out {branch}')

    try:
        repo.checkout(branch).wait()
    except ErrorReturnCode as e:
        print(f'Failed to checkout release branch {branch}. {e}')
        sys.exit(1)


def find_tag_version(repo, version: str) -> str:
    """
    Finds the latest tag for the provided version.
    This is done by iterating over the tags in the repository, checking against the provided major and minor versions
    and using the highest patch number found once the iteration is done.

    Parameters:
        repo: An sh.Command baked for git on the working repository.
        version: The major and minor versions we want to create a new tag for in the format 'M.m'

    Returns:
        The new tag to be created.
    """
    patch_version = -1
    version_regex = re.compile(fr'^{re.escape(version)}\.(\d+)$')

    for tag in repo.tag().splitlines():
        matched = version_regex.match(tag)
        if matched:
            patch = int(matched[1])
            if patch > patch_version:
                patch_version = patch

    if patch_version == -1:
        print(f'Failed to find an existing tag for {".".join(version)}')
        sys.exit(1)

    return f'{version}.{patch_version + 1}'


def create_empty_commit(repo):
    """
    Creates an empty commit on the current branch. Uses defaults for author, signature, etc.
    """
    print('Creating empty commit.')
    try:
        repo.commit('--allow-empty', '-m', 'Empty commit')
    except ErrorReturnCode as e:
        print(f'Failed to create empty commit: {e}')
        sys.exit(1)


def create_new_tag(repo, new_tag: str):
    """
    Creates a new tag on the current commit.

    Parameters:
        new_tag: The new tag to be created. i.e: 3.8.5
    """
    print(f'Creating new tag: {new_tag}')
    try:
        git.tag(new_tag)
    except ErrorReturnCode as e:
        print(f'Failed to create new tag {new_tag}. {e}')
        sys.exit(1)


def push_branch(repo):
    """
    Executes a push on the current branch.

    Parameters:
        repo: An sh.Command baked for git on the working repository.
    """
    print('Pushing release branch...')
    try:
        repo.push()
    except ErrorReturnCode as e:
        print(f'Failed to push empty commit to release branch. {e}')
        sys.exit(1)


def push_tag(repo, new_tag: str, remote: str):
    """
    Push a new tag to the provided remote.

    Parameters:
        repo: An sh.Command baked for git on the working repository.
        new_tag: The new tag to be pushed. i.e: 3.8.5
        remote: The remote in the repository the tag will be pushed to. i.e: origin
    """
    print(f'Pushing {new_tag} to {remote}...')
    try:
        repo.push(remote, new_tag)
    except ErrorReturnCode as e:
        print(f'Failed to push tag {new_tag} to {remote}. {e}')


def main(version: str, dry_run: bool, push: bool, path: str, remote: str):
    repo = get_repo_handle(path)

    new_tag = find_tag_version(repo, version)

    print(f'New tag to be created: {new_tag}')

    if dry_run:
        print('This is a dry run, no tag created.')
        return

    # Before doing anything else, ensure branch rolls back to current working one
    atexit.register(exit_handler, repo)

    fetch_all(repo)
    checkout_release_branch(repo, version)
    create_empty_commit(repo)
    create_new_tag(repo, new_tag)

    if not push:
        print(f'Created empty commit and new tag {new_tag}')
        print("Run")
        print(f"git checkout {get_release_branch(version)} && git push && git push {remote} {new_tag}")
        print("to publish them")
        return

    push_branch(repo)
    push_tag(repo, new_tag)


if __name__ == "__main__":
    description = """Creates a new patch tag with an empty commit.
Useful when we need to simply rebuild a collector image."""
    parser = argparse.ArgumentParser(description=description,
                                     formatter_class=argparse.RawTextHelpFormatter)

    parser.add_argument('version', help='Version to bump in the vormat X.Y', type=validate_version)
    parser.add_argument('-d', '--dry-run', help='Run all checks without actually modifying the repo',
                        default=False, action='store_true')
    parser.add_argument('-p', '--push', help="Push the newly create tag", default=False, action='store_true')
    parser.add_argument('-C', '--cwd',
                        help='Path to the repository to run in, defaults to the directory this script is in',
                        default='')
    parser.add_argument('-r', '--remote', help="Remote repoditory to push tags to, defaults to 'origin'")
    args = parser.parse_args()

    version = args.version
    dry_run = args.dry_run
    push = args.push
    path = args.cwd
    remote = args.remote

    main(version, dry_run, push, path, remote)
