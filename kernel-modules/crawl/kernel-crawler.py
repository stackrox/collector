#!/usr/bin/env python

#### LICENSING:
#### This file is derived from sysdig, in scripts/kernel-crawler.py.
#### Sysdig is licensed under the GNU General Public License v2.
#### This file is not distributed with StackRox code, and is only
#### used during compilation.
####
#### Original attribution notice:
# Author: Samuele Pilleri
# Date: August 17th, 2015
#### END LICENSING

import argparse
import json
import sys
import urllib.request as urllib2
from lxml import html
import traceback

#
# This is the main configuration tree for easily analyze Linux repositories
# hunting packages. When adding repos or so be sure to respect the same data
# structure
#
centos_excludes = [
    "3.10.0-123", # 7.0.1406
    "3.10.0-229", # 7.1.1503
    "3.10.0-327", # 7.2.1511
]
ubuntu_excludes = [
    "4.15.0-14", # SROX-11665 will remove this exclusion
    "4.17.0", # The module does not compile on 4.17 yet.
    "4.18.0", # The module does not compile on 4.18 yet.
]
ubuntu_backport_excludes = [
    "~", # prevent duplicate backports from cluttering the list
]
debian_excludes = [
    "4.17.0", # 4.17+ are not supported yet
    "4.18.0",
    "4.19.0",
    "3.2.0", "3.16.0" # legacy
]
repos = {
    "CentOS" : [
        {
            # This is the root path of the repository in which the script will
            # look for distros (HTML page)
            "root" : "https://mirrors.kernel.org/centos/",

            # This is the XPath + Regex (optional) for analyzing the `root`
            # page and discover possible distro versions. Use the regex if you
            # want to limit the version release.
            # Here, we want subpaths like /7.5.1804/. The path /7/ is always
            # an alias to the latest release, so there is no use crawling it.
            "discovery_pattern" : "/html/body//pre/a[regex:test(@href, '^7\..*$')]/@href",

            # Once we have found every version available, we need to know were
            # to go inside the tree to find packages we need (HTML pages)
            "subdirs" : [
                "os/x86_64/Packages/",
                "updates/x86_64/Packages/"
            ],

            # Finally, we need to inspect every page for packages we need.
            # Again, this is a XPath + Regex query so use the regex if you want
            # to limit the number of packages reported.
            "page_pattern" : "/html/body//a[regex:test(@href, '^kernel-devel-?[0-9].*\.rpm$')]/@href",

            # Exclude old versions we choose not to support.
            "exclude_patterns": centos_excludes
        },

        {
            # CentOS Vault hosts packages that are no longer available on
            # the up-to-date mirrors.
            # Sysdig also crawls http://vault.centos.org/centos/, but that
            # appears to be completely redundant.
            "root" : "http://vault.centos.org/",
            # Here, we want subpaths like /7.5.1804/. The path /7/ is always
            # an alias to the latest release, so there is no use crawling it.
            "discovery_pattern" : "//body//table/tr/td/a[regex:test(@href, '^7\..*$')]/@href",

            "subdirs" : [
                "os/x86_64/Packages/",
                "updates/x86_64/Packages/"
            ],
            "page_pattern" : "//body//table/tr/td/a[regex:test(@href, '^kernel-devel-?[0-9].*\.rpm$')]/@href",
            "exclude_patterns": centos_excludes
        },

        {
            # All kernels released to the main ELRepo repo also end up in the
            # archive, so it's OK just to crawl the archive.
            # However, archives do eventually drop packages; track those in
            # centos-uncrawled.txt.
            "root" : "http://ftp.utexas.edu/elrepo/archive/kernel/",
            "discovery_pattern" : "//body//table/tr/td/a[regex:test(@href, '^el7.*$')]/@href",
            "subdirs" : [
                "x86_64/RPMS/"
            ],
            "page_pattern" : "//body//table/tr/td/a[regex:test(@href, '^kernel-lt-devel-?[0-9].*\.rpm$')]/@href"
        }
    ],

    "CoreOS" : [
        # {
        #     "root" : "http://alpha.release.core-os.net/",
        #     "discovery_pattern": "/html/body//a[regex:test(@href, 'amd64-usr')]/@href",
        #     "subdirs" : [
        #         ""
        #     ],
        #     "page_pattern" : "/html/body//a[regex:test(@href, '^[5-9][0-9][0-9]|current|[1][0-9]{3}')]/@href"
        # Note: ^[5-9][0-9][0-9] is excluded because versions under 1000 are so old.
        # },

        # {
        #     "root" : "http://beta.release.core-os.net/",
        #     "discovery_pattern": "/html/body//a[regex:test(@href, 'amd64-usr')]/@href",
        #     "subdirs" : [
        #         ""
        #     ],
        #     "page_pattern" : "/html/body//a[regex:test(@href, '^[1][0-9]{3}')]/@href"
        #     # Note: ^[5-9][0-9][0-9] is excluded because versions under 1000 are so old.
        # },

        {
            "root" : "http://stable.release.core-os.net/",
            "discovery_pattern": "/html/body//a[regex:test(@href, '^amd64-usr/$')]/@href",
            # Note: ^[4-9][0-9][0-9] is excluded because versions under 1000 are so old.
            "subdirs" : [
                ""
            ],
            "page_pattern" : "/html/body//a[regex:test(@href, '^1185|1[2-9][0-9]{2}')]/@href"
        }
    ],
    "Debian": [
        {
            "root": "http://ftp.us.debian.org/debian/pool/main/l/",
            "discovery_pattern": "/html/body//a[regex:test(@href, '^linux/')]/@href",
            "subdirs": [""],
            "page_pattern": "/html/body//a[regex:test(@href, '^linux-(?:headers-[0-9.]+-[^-]+-(?:amd64|common_)|kbuild-.*_4.9.130).*(?:amd64|all).deb$')]/@href",
            "exclude_patterns": debian_excludes,
        },
    ],

    "Ubuntu": [
        # Generic Linux AMD64 headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*-generic.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # Generic Linux "all" headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*_all.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        }
    ],
    "Ubuntu-Azure": [
        # linux-azure AMD64 headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-azure/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*-azure.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes + ubuntu_backport_excludes
        },

        # linux-azure "all" headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-azure/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-azure-headers-[4-9].*_all.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes + ubuntu_backport_excludes
        },
    ],
    "Ubuntu-AWS": [
        # linux-aws AMD64 headers, distributed from universe (older versions only)
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-aws/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*-aws.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # linux-aws "all" headers, distributed from universe (older versions only)
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-aws/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-aws-headers-[4-9].*_all.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # linux-aws AMD64 headers, distributed from main (newer versions only)
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-aws/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*-aws.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # linux-aws "all" headers, distributed from main (newer versions only)
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-aws/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-aws-headers-[4-9].*_all.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        }
    ],
    "Ubuntu-GCP": [
        # linux-gcp AMD64 headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-gcp/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*-gcp.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes + ubuntu_backport_excludes
        },

        # linux-gcp "all" or amd64 headers, distributed from main.
        # Only amd64 packages have been published for recent kernel versions.
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-gcp/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-gcp-headers-[4-9].*_(all|amd64).deb$')]/@href",
            "exclude_patterns": ubuntu_excludes + ubuntu_backport_excludes
        },

        # linux-gcp AMD64 headers, distributed from universe
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-gcp/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*-gcp.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes + ubuntu_backport_excludes
        },

        # linux-gcp "all" headers, distributed from universe.
        # As of 4.15.0-1014, "all" is not uploaded, but "amd64" is.
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-gcp/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-gcp-headers-[4-9].*_(all|amd64).deb$')]/@href",
            "exclude_patterns": ubuntu_excludes + ubuntu_backport_excludes
        },
    ],
    "Ubuntu-GKE": [
        # linux-gke AMD64 headers, distributed from universe
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-gke/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*-gke.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # linux-gke "all" headers, distributed from universe
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-gke/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-gke-headers-[4-9].*_all.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },
    ]
}


def crawl(distro):
    """
    Navigate the `repos` tree and look for packages we need that match the
    patterns given.
    """
    URL_TIMEOUT = 30

    kernel_urls = []
    for repo in repos[distro]:
        sys.stderr.write("Considering repo " + repo["root"] + "\n")
        try:
            root = urllib2.urlopen(repo["root"], timeout=URL_TIMEOUT).read()
            versions = html.fromstring(root).xpath(repo["discovery_pattern"],
                                                   namespaces={"regex": "http://exslt.org/regular-expressions"})
            for version in versions:
                sys.stderr.write("Considering version "+version+"\n")
                for subdir in repo["subdirs"]:
                    try:
                        sys.stderr.write("Considering version " + version + " subdir " + subdir + "\n")
                        source = repo["root"] + version + subdir
                        page = urllib2.urlopen(source, timeout=URL_TIMEOUT).read()
                        rpms = html.fromstring(page).xpath(repo["page_pattern"],
                                                           namespaces={"regex": "http://exslt.org/regular-expressions"})
                        if len(rpms) == 0:
                            sys.stderr.write("WARN: Zero packages returned for version " + version + " subdir " + subdir + "\n")
                        for rpm in rpms:
                            sys.stderr.write("Considering package " + rpm + "\n")
                            if "exclude_patterns" in repo and any(x in rpm for x in repo["exclude_patterns"]):
                                continue
                            else:
                                sys.stderr.write("Adding package " + rpm + "\n")
                                kernel_urls.append("{}{}".format(source, urllib2.unquote(rpm)))
                    except urllib2.HTTPError as e:
                        sys.stderr.write("WARN: Error for source: {}: {}\n".format(source, e))

        except Exception as e:
            sys.stderr.write("ERROR: "+str(type(e))+str(e)+"\n")
            traceback.print_exc()
            sys.exit(1)

    return kernel_urls


def read_kernel_urls_from_stdin():
    input_urls = []
    for line in sys.stdin:
        input_urls.append(line.strip())

    return input_urls


def sort_and_output(urls):
    # For consistency with what was done before, sort URLs based on their alphabetical order
    #  _after_ reversing each of them.
    sorted_urls = sorted(urls, key=lambda s: s[::-1])
    print("\n".join(sorted_urls))


def handle_crawl(args):
    crawled_urls = crawl(args.distro)
    if not args.preserve_removed_urls:
        sort_and_output(crawled_urls)
        return

    input_urls = read_kernel_urls_from_stdin()
    if len(input_urls) == 0:
        raise Exception("Preserve removed urls was selected, but we couldn't read any urls from stdin!")

    removed_urls = [url for url in input_urls if url not in set(crawled_urls)]
    urls_dict = {
        "removed": removed_urls,
        "crawled": crawled_urls,
    }
    print(json.dumps(urls_dict))


def handle_output_from_json(args):
    urls_dict = json.loads(sys.stdin.read())
    sort_and_output(urls_dict[args.mode])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Kernel module crawler.")

    subparsers = parser.add_subparsers()

    parser_crawl = subparsers.add_parser("crawl", help="Crawl modules")
    parser_crawl.add_argument("distro", choices=repos.keys(), help="The distro you want to crawl for.")
    parser_crawl.add_argument("--preserve-removed-urls", action="store_true",
                              help="Use this option to pass the old list of kernels via stdin, and get a JSON "
                                   "with both new kernels and old kernels")
    parser_crawl.set_defaults(func=handle_crawl)

    parser_output_from_json = subparsers.add_parser("output-from-json",
                                                    help="Output url files based on a stored JSON from a previous "
                                                         "invocation of crawl.")
    parser_output_from_json.add_argument(
        "mode", choices=["crawled", "removed"],
        help="Do you want to print the newly crawled files, or update the 'uncrawled' file?")
    parser_output_from_json.set_defaults(func=handle_output_from_json)

    args = parser.parse_args()
    args.func(args)
