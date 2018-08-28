#!/usr/bin/env python3

import argparse
import gzip
import hashlib
import os
import os.path
import subprocess
import sys
import tempfile
import urllib.request
from lxml import objectify
import sqlite3

class DigestValidator:
    def __init__(self, digest, alg="sha256"):
        self._h = hashlib.new(alg)
        self._known = digest
    def set_file(self, f):
        self._name = f
        self._h.update(open(f, 'rb').read())
        return self
    def set_data(self, buf, name):
        self._name = name
        self._h.update(buf)
        return self
    def check(self):
        return self._h.hexdigest() == self._known
    def do_assert(self):
        if not self.check():
            raise AssertionError("{2} failed digest check!\nExpected {0} but got {1}".format(self._known, self._h.hexdigest(), self._name))

class GPGValidator:
    def __init__(self, keyfile):
        self._keyfile = keyfile.name
    def with_data(self, data, sig):
        self.data = data
        self.sig = sig
        return self
    def check(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            #gpg = ['gpg', '--no-default-keyring','--keyring',
            #       os.path.join(tmpdir, 'keyring')]
            #subprocess.check_call(gpg + ["--import", self._keyfile])
            dataf = os.path.join(tmpdir, 'data')
            sigf = dataf + '.gpg'
            open(dataf, 'wb').write(self.data)
            open(sigf, 'wb').write(self.sig)
            ret = subprocess.call(['gpgv', '--keyring', os.path.realpath(self._keyfile), sigf, dataf])
            return ret == 0
    def do_assert(self):
        if not self.check():
            raise AssertionError("GPG signature did not validate!")

class HttpUser:
    def _fetch(self, uri, decode = None, sha256 = None):
        res = urllib.request.urlopen(uri)
        data = res.read()
        if sha256:
            DigestValidator(sha256).set_data(data, uri).do_assert()
        if decode:
            return decode(data)
        return data

class AptRepo(HttpUser):
    def __init__(self, keyfile, uri, dist, arch, *components):
        self.keyfile = keyfile
        self.uri = uri
        self.dist = dist
        self.arch = arch
        self.components = components
        self.packages = {}
        self._rel_data = self._get_release()

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
            GPGValidator(self.keyfile).with_data(
                rel, self._fetch(link + '.gpg')
            ).do_assert()
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

    def find_packages(self, matchers):
        for c in self.components:
            if not c in self.packages:
                pkg = '{1}/{0.arch}/Packages.gz'.format(self, c)
                link = '{0.uri}/dists/{0.dist}/{1}'.format(self, pkg)
                sha256 = self._rel_data[pkg]['SHA256']
                self.packages[c] = list(
                    self.parse_package(self._fetch(link, self._gunzip, sha256=sha256)))
            for pack in self.packages[c]:
                if all(m(pack) for m in matchers):
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
            if DigestValidator(digest).set_file(outf).check():
                print("{} already downloaded".format(outf))
                return
            else:
                print("{} exists but has incorrect digest -- redownloading.".format(outf))
        elif not os.path.exists(outdir):
            raise ValueError("Path does not exist: {}".format(outdir))
        url = self.url(pkg)
        print("Downloading {0} to {1}".format(url, outdir))
        urllib.request.urlretrieve(url, outf, self._reporthook(url))
        DigestValidator(digest).set_file(outf).do_assert()

def is_dir(x):
    if os.path.isdir(x):
        return x
    raise argparse.ArgumentTypeError("Directory %s not found" % x)

class PrefixMatch:
    def __init__(self, k, v):
        self.k = k
        self.v = v
    def __call__(self, pkg):
        return pkg[self.k].startswith(self.v)
    def __repr__(self):
        return 'prefix_match({0.k!r},{0.v!r})'.format(self)
    def sqlmatch(self):
        return '"{0}" like ?'.format(self.k)
    def sqlval(self):
        return self.v + '%'

class ExactMatch:
    def __init__(self, k, v):
        self.k = k
        self.v = v
    def __call(self, pkg):
        return pkg[self.k] == self.v
    def __repr__(self):
        return "exact_match({0.k!r},{0.v!r})".format(self)
    def sqlmatch(self):
        return '"{0}" = ?'.format(self.k)
    def sqlval(self):
        return self.v

def prefix_match(kv):
    (k,v) = kv.split("=", 1)
    return PrefixMatch(k,v)

def exact_match(kv):
    (k,v) = kv.split("=", 1)
    return ExactMatch(k,v)

class YumRepo(HttpUser):
    def __init__(self, gpg_key, mirror_list_uri):
        self.gpg_key = gpg_key
        self.mirrors = mirror_list_uri

    def _uri(self, path):
        return urllib.parse.urljoin(self.base, path)

    def _query(self, db, sql, args):
        def dict_factory(cursor, row):
            d = {}
            for idx, col in enumerate(cursor.description):
                d[col[0]] = row[idx]
            return d
        db = sqlite3.connect(db)
        db.row_factory = dict_factory
        cur = db.cursor()
        cur.execute(sql, args)
        for some in iter(cur.fetchmany, []):
            for r in some:
                yield r

    def maybe_download(self, outf, uri, digest):
        if os.path.exists(outf) and DigestValidator(digest).set_file(outf).check():
            return
        data = self._fetch(uri)
        open(outf, 'wb').write(data)
        DigestValidator(digest).set_file(outf).do_assert()

    def find_packages(self, matchers):
        mirror = self._fetch(self.mirrors).split(b'\n')[0].decode('utf-8')
        self.base = mirror + '/'
        mduri = self._uri('repodata/repomd.xml')
        repomd = self._fetch(mduri)
        if self.gpg_key:
            GPGValidator(self.gpg_key).with_data(
                repomd,
                self._fetch(mduri + '.asc')
            ).do_assert()

        md = objectify.fromstring(repomd)
        data = {}
        for x in md.data:
            val = {}
            val['type'] = x.get('type')
            val['href'] = x.location.get("href")
            val['digest'] = x.checksum.text
            data[val['type']] = val
        self.maybe_download(
            'primary.db.gz',
            self._uri(data['primary_db']['href']),
            data['primary_db']['digest'])
        subprocess.check_call(["gunzip", "-fk", "primary.db.gz"])
        query = "select * from packages"
        args = []
        if matchers:
            query += " WHERE " + (" AND ".join(m.sqlmatch() for m in matchers))
            args = [m.sqlval() for m in matchers]
        for row in self._query('primary.db', query, args):
            yield row
    def url(self, pkg):
        return self._uri(pkg['location_href'])

def main():
    root = argparse.ArgumentParser()
    root.add_argument("--gpg-key", metavar="keyfile", type=argparse.FileType("r"), help="GPG signing key to use to verify", default=None)
    action = root.add_mutually_exclusive_group(required=True)
    action.add_argument("--download", metavar="output_dir", action="store", help="download directory")
    action.add_argument("--print", action="store_true", help="print list of URLs")
    subcommands = root.add_subparsers()

    apt = subcommands.add_parser("apt")
    apt.add_argument("--arch", help="architecture to use (default binary-amd64)", default="binary-amd64")
    apt.add_argument("apt_uri", help="URI to the root of the apt repository",metavar="URI")
    apt.add_argument("dist", help="dist name in the apt repository")
    apt.add_argument("component", nargs="+", help="repository components to use")

    yum = subcommands.add_parser("yum")
    yum.add_argument("yum_uri", help="URI to the mirrors.list of the YUM repository", metavar="mirrorURI")

    for x in (yum, apt):
        x.add_argument("--match-prefix", help="Key=Value pattern to match packages to act on", dest="matchers", type=prefix_match, action="append", default=[])
        x.add_argument("--match-exact", help="Key=Value pattern to exactly match package values", dest="matchers", type=exact_match, action="append")

    args = root.parse_args()
    if 'apt_uri' in args:
        repo = AptRepo(
            args.gpg_key, args.apt_uri, args.dist, args.arch, *args.component)
    elif 'yum_uri' in args:
        repo = YumRepo(
            args.gpg_key, args.yum_uri)
    else:
        root.usage()
        sys.exit(2)

    packages = list(repo.find_packages(args.matchers))

    if args.print:
        for pkg in packages:
            print(repo.url(pkg))

    if args.download:
        for pkg in packages:
            repo.download(args.download, pkg)

if __name__ == '__main__':
    main()
