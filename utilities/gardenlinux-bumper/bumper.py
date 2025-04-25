#!/usr/bin/env python3

import argparse
import os
import re

# gfm stands for Github Flavoured Markdown
from marko.ext.gfm import gfm

from bs4 import BeautifulSoup

import requests

type GardenVersion = tuple[str, int, int, str]


def get_latest_release() -> GardenVersion:
    """
    Find the Garden Linux version in a GH release.

    This method queries the GH API for the latest release of gardenlinux
    and attempts to extract the version for the gcp image from it.

    The block that holds the image name looks something like this in
    markdown:
    ### Google Cloud Platform (amd64)
    ```
    gcp_image_name: gardenlinux-gcp-8bd7b82716e90c634df25013-1592-9-2eaf0fc6
    ```

    marko parses the markdown string and translates it to equivalent
    html that looks something like this:
    <h3>Google Cloud Platform (amd64)</h3>
    <pre><code>gcp_image_name: gardenlinux-gcp-8bd7b82716e90c634df25013-1592-9-2eaf0fc6</code></pre>

    We can then use BeautifulSoup to parse the html and look for the
    data we want. A bit convoluted, but it does not looke like we can do
    the same with marko directly.
    """
    gcp_vm_re = re.compile(
        r'^gcp_image_name: gardenlinux-gcp-([0-9a-f]+)-([0-9]+)-([0-9]+)-([0-9a-f]+)$')
    url = 'https://api.github.com/repos/gardenlinux/gardenlinux/releases/latest'
    headers = {
        'Accept': 'application/vnd.github+json',
        'Authorization': f"Bearer {os.environ['GITHUB_TOKEN']}",
        'X-GitHub-Api-Version': '2022-11-28',
    }
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    latest_release = response.json()

    body = gfm.convert(latest_release['body'])
    body = BeautifulSoup(body, features='html.parser')
    h = body.find(name='h3', string='Google Cloud Platform (amd64)')
    if h is None:
        raise RuntimeError('Failed to find the GCP VM image: Missed <h3>')

    pre = h.find_next_sibling('pre')
    if pre is None:
        raise RuntimeError('Failed to find the GCP VM image: Missed <pre>')

    match = gcp_vm_re.match(pre.code.string.strip())
    if match is None:
        raise RuntimeError(
            'Failed to find the GCP VM image: Version did not match')

    checksum = match[1]
    major = int(match[2])
    minor = int(match[3])
    commit = match[4]

    return (checksum, major, minor, commit)


def get_current_version(image_file: str) -> GardenVersion:
    gcp_vm_re = re.compile(
        r'^gardenlinux-gcp-([-0-9a-z]+)-([0-9]+)-([0-9]+)-([0-9a-f]+)$')

    with open(image_file, 'r') as f:
        image = f.readline()
        match = gcp_vm_re.match(image)
        if match is None:
            raise RuntimeError('Configured image did not match')

        checksum = match[1]
        major = int(match[2])
        minor = int(match[3])
        commit = match[4]

        return (checksum, major, minor, commit)


def get_gardenlinux_image(version_data: GardenVersion) -> str:
    checksum, major, minor, commit = version_data

    return f'gardenlinux-gcp-{checksum}-{major}-{minor}-{commit}'


def image_is_outdated(latest: GardenVersion, current: GardenVersion) -> bool:
    return latest[1:3] > current[1:3]


def main(image_file: str):
    latest = get_latest_release()
    current = get_current_version(image_file)

    if image_is_outdated(latest, current):
        with open(image_file, 'w') as f:
            f.write(get_gardenlinux_image(latest))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('filename')
    args = parser.parse_args()

    main(args.filename)
