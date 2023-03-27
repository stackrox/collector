#!/usr/bin/env python3

import argparse
import os
import re
import requests


def get_latest_release() -> (int, int, str):
    gcp_vm_re = re.compile(r'^gcp-gardener_prod-amd64-([0-9]+)\.([0-9]+)-([0-9a-f]+)\.tar\.xz$')
    url = "https://api.github.com/repos/gardenlinux/gardenlinux/releases/latest"
    headers = {
        'Accept': 'application/vnd.github+json',
        'Authorization': f'Bearer {os.environ["GITHUB_TOKEN"]}',
        'X-GitHub-Api-Version': '2022-11-28',
    }
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    latest_release = response.json()

    for a in latest_release['assets']:
        match = gcp_vm_re.match(a['name'])
        if match:
            major = int(match[1])
            minor = int(match[2])
            checksum = match[3]

            return (major, minor, checksum)
    raise RuntimeError("Failed to find the GCP VM asset")


def get_current_version(image_file: str) -> (int, int, str):
    gcp_vm_re = re.compile(r'^gardenlinux-gcp-gardener-prod-amd64-([0-9]+)-([0-9]+)-([0-9a-f]+)$')

    with open(image_file, 'r') as f:
        image = f.readline()
        match = gcp_vm_re.match(image)
        if match is None:
            raise RuntimeError('Configured image did not match')

        major = int(match[1])
        minor = int(match[2])
        checksum = match[3]

        return (major, minor, checksum)


def get_gardenlinux_image(version_data: (int, int, str)) -> str:
    major = version_data[0]
    minor = version_data[1]
    checksum = version_data[2]

    return f'gardenlinux-gcp-gardener-prod-amd64-{major}-{minor}-{checksum}'


def image_is_outdated(latest: (int, int, str), current: (int, int, str)):
    latest_major = latest[0]
    latest_minor = latest[1]

    current_major = current[0]
    current_minor = current[1]

    return latest_major > current_major or (latest_major == current_major and latest_minor > current_minor)


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
