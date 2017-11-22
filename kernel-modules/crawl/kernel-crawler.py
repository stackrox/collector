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
            "root" : "http://ftp.utexas.edu/elrepo/kernel/",
            "discovery_pattern" : "//body//table/tr/td/a[regex:test(@href, '^el7.*$')]/@href",
            "subdirs" : [
                "x86_64/RPMS/"
            ],
            "page_pattern" : "//body//table/tr/td/a[regex:test(@href, '^kernel-lt-(devel-)?[0-9].*\.rpm$')]/@href"
        },
        {
            "root" : "http://ftp.utexas.edu/elrepo/archive/kernel/",
            "discovery_pattern" : "//body//table/tr/td/a[regex:test(@href, '^el7.*$')]/@href",
            "subdirs" : [
                "x86_64/RPMS/"
            ],
            "page_pattern" : "//body//table/tr/td/a[regex:test(@href, '^kernel-lt-(devel-)?[0-9].*\.rpm$')]/@href"
        }
    ],
}

#
# In our design you are not supposed to modify the code. The whole script is
# created so that you just have to add entry to the `repos` array and new
# links will be found automagically without needing to write any single line of
# code.
#
URL_TIMEOUT=30

if len(sys.argv) < 2 or not (sys.argv[1] in repos or sys.argv[1] in ["Ubuntu", "Debian"]):
    sys.stderr.write("Usage: " + sys.argv[0] + " <distro>\n")
    sys.exit(1)

distro = sys.argv[1]

def get_gzipped_data(url):
    response = urllib2.urlopen(url, timeout=URL_TIMEOUT)
    if response.info().get('Content-Encoding') == 'x-gzip' or response.info().get('Content-Encoding') == 'gzip':
        buf = StringIO(response.read())
        f = gzip.GzipFile(fileobj=buf)
        data = f.read()
    else:
        data = response.read()
    return data

def print_ubuntu_deb_urls(version, arch, pkg):
    download_url = "https://packages.ubuntu.com/%s/%s/%s/download" % (version, arch, pkg)
    sys.stderr.write("Looking for download URLs for pkg: "+pkg+" at "+download_url+"\n")
    download_page = requests.get(download_url)
    html_pattern = "/html/body//a[regex:test(@href, '^(http://mirrors.kernel.org/ubuntu|http://security.ubuntu.com)')]/@href"
    pkg_urls = html.fromstring(download_page.text).xpath(html_pattern, namespaces = {"regex": "http://exslt.org/regular-expressions"})
    if len(pkg_urls) == 0:
        sys.stderr.write("WARN: Zero packages returned for version " + version + " arch " + arch + " pkg " + pkg + "\n")
    for url in pkg_urls:
        sys.stderr.write("pkg: "+pkg+" got URL: "+url+"\n")
        print url

def get_debian_deb_urls(version, arch, pkg):
    download_url = "https://packages.debian.org/%s/%s/%s/download" % (version, arch, pkg)
    sys.stderr.write("Looking for download URLs for pkg: "+pkg+" at "+download_url+"\n")
    download_page = requests.get(download_url)
    html_pattern = "/html/body//a[regex:test(@href, '^(http://(http.us|security).debian.org/debian)')]/@href"
    pkg_urls = html.fromstring(download_page.text).xpath(html_pattern, namespaces = {"regex": "http://exslt.org/regular-expressions"})
    if len(pkg_urls) == 0:
        sys.stderr.write("WARN: Zero packages returned for version " + version + " arch " + arch + " pkg " + pkg + "\n")
    for url in pkg_urls:
        sys.stderr.write("pkg: "+pkg+" got URL: "+url+"\n")
    return pkg_urls

def get_ubuntu_kernels(pkg, pattern, kernels_handled, get_fn):
    match = pattern.match(pkg)
    if match:
        kernel_rev = match.group(1)

        kernel_parts = kernel_rev.split(".")
        if len(kernel_parts) >= 2:
            if int(kernel_parts[0]) == 4 and int(kernel_parts[1]) > 11:
                sys.stderr.write("Ignoring kernel >4.11: ("+kernel_rev+") in package "+pkg+"\n")
                return
            if int(kernel_parts[0]) < 4 or (int(kernel_parts[0]) == 4 and int(kernel_parts[1]) < 4):
                sys.stderr.write("Ignoring kernel before 4.4: ("+kernel_rev+") in package "+pkg+"\n")
                return

        kernel_rev_abi = kernel_rev.split("_")[0]
        if kernels_handled.get(kernel_rev_abi):
            sys.stderr.write("Ignoring kernel release "+kernel_rev_abi+" that is already handled\n")
            return

        kernels_handled[kernel_rev_abi] = True

        get_fn(pkg, kernel_rev)

def print_ubuntu_standard_packages(pkg, kernel_rev):
    # The headers-all package
    print_ubuntu_deb_urls(version, "all", pkg)

    # The headers-generic package
    print_ubuntu_deb_urls(version, "amd64", pkg+"-generic")

    # The image package
    print_ubuntu_deb_urls(version, "amd64", pkg.replace('headers', 'image')+"-generic")

def print_ubuntu_gke_packages(pkg, kernel_rev):
    # The headers-all package
    print_ubuntu_deb_urls(version, "all", "linux-gke-headers-" + kernel_rev)

    # The headers-generic package
    print_ubuntu_deb_urls(version, "amd64", pkg)

    # The image package
    print_ubuntu_deb_urls(version, "amd64", pkg.replace('headers', 'image'))

def print_ubuntu_aws_packages(pkg, kernel_rev):
    # The headers-all package
    print_ubuntu_deb_urls(version, "all", "linux-aws-headers-" + kernel_rev)

    # The headers-generic package
    print_ubuntu_deb_urls(version, "amd64", pkg)

    # The image package
    print_ubuntu_deb_urls(version, "amd64", pkg.replace('headers', 'image'))

def print_ubuntu_azure_packages(pkg, kernel_rev):
    # The headers-all package
    print_ubuntu_deb_urls(version, "all", "linux-azure-headers-" + kernel_rev)

    # The headers-generic package
    print_ubuntu_deb_urls(version, "amd64", pkg)

    # The image package
    print_ubuntu_deb_urls(version, "amd64", pkg.replace('headers', 'image'))

if distro == "Ubuntu":
    # Ubuntu's package mirrors have packages from a large number of Ubuntu
    # versions. Since we only want to build kernel modules for versions we
    # have to support, we have to be a little more judicious. We do that by
    # checking the packages lists for the supported versions and then
    # getting the relevant .deb download URLs.
    versions = [
        "xenial",
        "xenial-updates",
        "trusty",
        "trusty-updates"
    ]

    std_pattern = re.compile(r"linux-headers-([0-9\.-]+)$")
    aws_pattern = re.compile(r"linux-headers-([0-9\.-]+)-aws$")
    azure_pattern = re.compile(r"linux-headers-([0-9\.-]+)-azure$")
    gke_pattern = re.compile(r"linux-headers-([0-9\.-]+)-gke$")

    std_kernels_handled = dict()
    aws_kernels_handled = dict()
    azure_kernels_handled = dict()
    gke_kernels_handled = dict()

    for version in versions:
        try:
            sys.stderr.write("** Processing packages for Ubuntu version: "+version+"\n")
            pkg_url = "https://packages.ubuntu.com/%s/allpackages?format=txt.gz" % version
            data = get_gzipped_data(pkg_url)
            for pkg in data.split('\n'):
                pkg = pkg.split(' ')[0]
                # sys.stderr.write("Considering pkg: "+pkg+"\n")
                get_ubuntu_kernels(pkg, std_pattern, std_kernels_handled, print_ubuntu_standard_packages)
                get_ubuntu_kernels(pkg, aws_pattern, aws_kernels_handled, print_ubuntu_aws_packages)
                get_ubuntu_kernels(pkg, azure_pattern, azure_kernels_handled, print_ubuntu_azure_packages)
                get_ubuntu_kernels(pkg, gke_pattern, gke_kernels_handled, print_ubuntu_gke_packages)
        except Exception as e:
            sys.stderr.write("ERROR: "+str(e)+"\n")
            traceback.print_exc()
            sys.exit(1)

elif distro == "Debian":
    # Debian's package mirrors have packages from a large number of Debian
    # versions. Since we only want to build kernel modules for versions we
    # have to support, we have to be a little more judicious. We do that by
    # checking the packages lists for the supported versions and then
    # getting the relevant .deb download URLs.
    versions = [
        "jessie",
        "jessie-updates"
    ]

    pattern = re.compile(r"linux-headers-([0-9\.-]+)-amd64$")
    kbuild_pattern = re.compile(r"linux-kbuild-([0-9\.-]+)$")

    kbuilds = set()
    kernels = set()

    for version in versions:
        try:
            sys.stderr.write("** Processing packages for Debian version: "+version+"\n")
            pkg_url = "https://packages.debian.org/%s/allpackages?format=txt.gz" % version
            data = get_gzipped_data(pkg_url)
            for pkg in data.split('\n'):
                pkg = pkg.split(' ')[0]
                # sys.stderr.write("Considering pkg: "+pkg+"\n")

                match = kbuild_pattern.match(pkg)
                if match:
                    kbuilds.update(get_debian_deb_urls(version, "amd64", pkg))
                    continue

                match = pattern.match(pkg)
                if match:
                    kernel_rev = match.group(1)

                    kernel_parts = kernel_rev.split(".")
                    if len(kernel_parts) >= 2:
                        if int(kernel_parts[0]) == 4 and int(kernel_parts[1]) > 11:
                            sys.stderr.write("Ignoring kernel >4.11: ("+kernel_rev+") in package "+pkg+"\n")
                            continue

                    # The headers-amd64 package
                    kernels.update(get_debian_deb_urls(version, "amd64", pkg))

                    # The headers-common package
                    kernels.update(get_debian_deb_urls(version, "amd64", pkg.replace('-amd64', '-common')))

                    # The image package
                    kernels.update(get_debian_deb_urls(version, "amd64", pkg.replace('headers', 'image')))
        except Exception as e:
            sys.stderr.write("ERROR: "+str(e)+"\n")
            traceback.print_exc()
            sys.exit(1)

        # The Debian build requires that kbuild packages be present
        # before the other packages for a given kernel, otherwise
        # the build is skipped.
        for kbuild in kbuilds:
            print kbuild
        for kernel in kernels:
            print kernel

else:
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
