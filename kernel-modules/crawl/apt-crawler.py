#!/usr/bin/env python3

import gzip
import hashlib
import os
import os.path
import subprocess
import sys
import tempfile
import urllib.request

class Validator:
    def __init__(self, digest, alg="sha256"):
        self._h = hashlib.new(alg)
        self._known = digest
    def set_file(self, f):
        self._name = f
        self._h.update(open(f, 'rb').read())
    def set_data(self, buf, name):
        self._name = name
        self._h.update(buf)
    def check(self):
        return self._h.hexdigest() == self._known
    def do_assert(self):
        if not self.check():
            raise AssertionError("{2} failed digest check!\nExpected {0} but got {1}".format(self._known, self._h.hexdigest(), self._name))

class AptRepo:
    def __init__(self, keyfile, uri, dist, arch, *components):
        self.keyfile = keyfile
        self.uri = uri
        self.dist = dist
        self.arch = arch
        self.components = components
        self.packages = {}
        self._rel_data = self._get_release()

    def _fetch(self, uri, decode = None, sha256 = None):
        res = urllib.request.urlopen(uri)
        data = res.read()
        if sha256:
            v = Validator(sha256)
            v.set_data(data, uri)
            v.do_assert()
        if decode:
            return decode(data)
        return data

    def _gunzip(self, data):
        return gzip.decompress(data).decode("utf-8")

    def parse_package(self, s):
        listing = s.split('\n\n')
        for l in listing:
            rec = {}
            prev = ''
            for line in l.split('\n'):
                if line.startswith(' '):
                    rec[prev] += line
                elif not line:
                    continue
                else:
                    k, v = line.split(": ", 1)
                    prev = k
                    rec[k] = v
            if not rec:
                return
            yield rec

    def _get_release(self):
        link = '{0.uri}/dists/{0.dist}/Release'.format(self)
        rel = self._fetch(link)
        if self.keyfile:
            subprocess.check_call(["gpg", "--import", self.keyfile])
            sig = self._fetch(link + '.gpg')
            relf = None
            sigf = None
            with tempfile.NamedTemporaryFile() as tsig, \
                 tempfile.NamedTemporaryFile() as trel:
                trel.write(rel)
                trel.flush()
                tsig.write(sig)
                tsig.flush()
                subprocess.check_call(
                    ["gpg", "--verify", tsig.name, trel.name])
        reldata = {}
        dfunc = ''
        for line in rel.decode('utf-8').split('\n'):
            if line[:3] in ('MD5', 'SHA'):
                # strip LF and :
                dfunc = line.strip()[:-1]
            if line[0:1] == ' ':
                digest,size,name = line.strip().split()
                dat = reldata.get(name, {})
                dat['name'] = name
                dat[dfunc] = digest
                dat['size'] = size
                reldata[name] = dat
        return reldata

    def find_packages(self, **args):
        for c in self.components:
            if not c in self.packages:
                pkg = '{1}/{0.arch}/Packages.gz'.format(self, c)
                link = '{0.uri}/dists/{0.dist}/{1}'.format(self, pkg)
                sha256 = self._rel_data[pkg]['SHA256']
                self.packages[c] = list(
                    self.parse_package(self._fetch(link, self._gunzip, sha256=sha256)))
            for pack in self.packages[c]:
                if all(pack[k].startswith(args[k]) for k in args):
                    yield pack
    def _reporthook(self, url):
        if not os.isatty(sys.stdout.fileno()):
            return
        def cb(num, size, total):
            cur = num * size
            s = 'Downloading {}... '.format(url)
            if total > 0:
                s += '{:.1f}%'.format(cur * 100.0/total)
            else:
                s += '{}k'.format(cur/1024)
            esc = '\033[{}D'.format(len(s))
            s += esc
            sys.stdout.write(s)
            sys.stdout.flush()
        return cb

    def url(self, pkg):
        return '{0.uri}/{1}'.format(self, pkg['Filename'])

    def download(self, outdir, pkg):
        outf = os.path.join(outdir, os.path.basename(pkg['Filename']))
        digest = pkg['SHA256']
        if os.path.exists(outf):
            v = Validator(digest)
            v.set_file(outf)
            if v.check():
                print("{} already downloaded".format(outf))
                return
            else:
                print("{} exists but has incorrect digest -- redownloading.".format(outf))
        elif not os.path.exists(outdir):
            raise ValueError("Path does not exist: {}".format(outdir))
        url = self.url(pkg)
        print("Downloading {0} to {1}".format(url, outdir))
        urllib.request.urlretrieve(url, outf, self._reporthook(url))
        v = Validator(digest)
        v.set_file(outf)
        v.do_assert()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: {0} output-directory".format(*sys.argv))
        sys.exit(2)
    kops = AptRepo(
        'kope.io.asc', 'http://dist.kope.io/apt', 'jessie', 'binary-amd64',
        'main')
    packages = list(kops.find_packages(Package='linux-headers-4'))
    for p in packages:
        print(kops.url(p))
        # kops.download(sys.argv[1], p)
