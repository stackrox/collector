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

import sys
import urllib2
from lxml import html
import requests
import gzip
from StringIO import StringIO
import re
import traceback

#
# This is the main configuration tree for easily analyze Linux repositories
# hunting packages. When adding repos or so be sure to respect the same data
# structure
#
centos_excludes = ["3.10.0-123", "3.10.0-229"]
ubuntu_excludes = []
repos = {
    "CentOS" : [
        {
            # This is the root path of the repository in which the script will
            # look for distros (HTML page)
            "root" : "http://mirrors.kernel.org/centos/",

            # This is the XPath + Regex (optional) for analyzing the `root`
            # page and discover possible distro versions. Use the regex if you
            # want to limit the version release
            "discovery_pattern" : "/html/body//pre/a[regex:test(@href, '^7.*$')]/@href",

            # Once we have found every version available, we need to know were
            # to go inside the tree to find packages we need (HTML pages)
            "subdirs" : [
                "os/x86_64/Packages/",
                "updates/x86_64/Packages/"
            ],

            # Finally, we need to inspect every page for packages we need.
            # Again, this is a XPath + Regex query so use the regex if you want
            # to limit the number of packages reported.
            "page_pattern" : "/html/body//a[regex:test(@href, '^kernel-(devel-)?[0-9].*\.rpm$')]/@href",

            # Exclude old versions we choose not to support.
            "exclude_patterns": centos_excludes
        },

        {
            "root" : "http://vault.centos.org/",
            "discovery_pattern" : "//body//table/tr/td/a[regex:test(@href, '^7.*$')]/@href",
            "subdirs" : [
                "os/x86_64/Packages/",
                "updates/x86_64/Packages/"
            ],
            "page_pattern" : "//body//table/tr/td/a[regex:test(@href, '^kernel-(devel-)?[0-9].*\.rpm$')]/@href",
            "exclude_patterns": centos_excludes
        },

        {
            "root" : "http://vault.centos.org/centos/",
            "discovery_pattern" : "//body//table/tr/td/a[regex:test(@href, '^7.*$')]/@href",
            "subdirs" : [
                "os/x86_64/Packages/",
                "updates/x86_64/Packages/"
            ],
            "page_pattern" : "//body//table/tr/td/a[regex:test(@href, '^kernel-(devel-)?[0-9].*\.rpm$')]/@href",
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
            "page_pattern" : "//body//table/tr/td/a[regex:test(@href, '^kernel-lt-(devel-)?[0-9].*\.rpm$')]/@href"
        }
    ],
    "Ubuntu": [
        # Generic Linux AMD64 image and headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-(image|headers)-[4-9].*-generic.*amd64.deb$')]/@href",
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
    "Ubuntu-HWE": [
        # linux-hwe AMD64 image and headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-hwe/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-(image|headers)-[4-9].*-generic.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # linux-hwe "all" headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-hwe/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*_all.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # linux-hwe-edge AMD64 image and headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-hwe-edge/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-(image|headers)-[4-9].*-generic.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # linux-hwe-edge "all" headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-hwe-edge/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-headers-[4-9].*_all.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        }
    ],
    "Ubuntu-Azure": [
        # linux-azure AMD64 image and headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-azure/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-(image|headers)-[4-9].*-azure.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # linux-azure "all" headers, distributed from main
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-azure/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-azure-headers-[4-9].*_all.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },
    ],
    "Ubuntu-AWS": [
        # linux-aws AMD64 image and headers, distributed from universe (older versions only)
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-aws/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-(image|headers)-[4-9].*-aws.*amd64.deb$')]/@href",
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

        # linux-aws AMD64 image and headers, distributed from main (newer versions only)
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/main/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-aws/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-(image|headers)-[4-9].*-aws.*amd64.deb$')]/@href",
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
        # linux-gcp AMD64 image and headers, distributed from universe
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-gcp/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-(image|headers)-[4-9].*-gcp.*amd64.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },

        # linux-gcp "all" headers, distributed from universe
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-gcp/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-gcp-headers-[4-9].*_all.deb$')]/@href",
            "exclude_patterns": ubuntu_excludes
        },
    ],
    "Ubuntu-GKE": [
        # linux-gke AMD64 image and headers, distributed from universe
        {
            "root" : "http://security.ubuntu.com/ubuntu/pool/universe/l/",
            "discovery_pattern" : "/html/body//a[@href = 'linux-gke/']/@href",
            "subdirs" : [""],
            "page_pattern" : "/html/body//a[regex:test(@href, '^linux-(image|headers)-[4-9].*-gke.*amd64.deb$')]/@href",
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

URL_TIMEOUT=30

if len(sys.argv) < 2 or not (sys.argv[1] in repos):
    sys.stderr.write("Usage: " + sys.argv[0] + " <distro>\n")
    sys.exit(1)

distro = sys.argv[1]

#
# Navigate the `repos` tree and look for packages we need that match the
# patterns given.
#
urls = set()
for repo in repos[distro]:
    try:
        root = urllib2.urlopen(repo["root"],timeout=URL_TIMEOUT).read()
        versions = html.fromstring(root).xpath(repo["discovery_pattern"], namespaces = {"regex": "http://exslt.org/regular-expressions"})
        for version in versions:
            sys.stderr.write("Considering version "+version+"\n")
            for subdir in repo["subdirs"]:
                try:
                    sys.stderr.write("Considering version " + version + " subdir " + subdir + "\n")
                    source = repo["root"] + version + subdir
                    page = urllib2.urlopen(source,timeout=URL_TIMEOUT).read()
                    rpms = html.fromstring(page).xpath(repo["page_pattern"], namespaces = {"regex": "http://exslt.org/regular-expressions"})
                    if len(rpms) == 0:
                        sys.stderr.write("WARN: Zero packages returned for version " + version + " subdir " + subdir + "\n")
                    for rpm in rpms:
                        sys.stderr.write("Considering package " + rpm + "\n")
                        if "exclude_patterns" in repo and any(x in rpm for x in repo["exclude_patterns"]):
                            continue
                        else:
                            sys.stderr.write("Adding package " + rpm + "\n")
                            print source + str(urllib2.unquote(rpm))
                except urllib2.HTTPError as e:
                    if e.code == 404:
                        sys.stderr.write("WARN: "+str(e)+"\n")
    except Exception as e:
        sys.stderr.write("ERROR: "+str(type(e))+str(e)+"\n")
        traceback.print_exc()
        sys.exit(1)
