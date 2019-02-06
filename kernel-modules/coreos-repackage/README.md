# 📦 CoreOS Kernel Distribution Repackager

## Motivations

CoreOS distributes their kernel headers bundled inside of a ["CoreOS development container file"](https://coreos.com/os/docs/latest/kernel-modules.html#prepare-a-coreos-container-linux-development-container). Consuming this file directly during kernel builds, while sufficient, is awkward for a number of reasons:

- Large artifact size.
- Long unpack times.
- Privileged Docker runtime requirements.

This repo contains tooling for pre-processing and repackaging this artifact into something that is more consumable for the build system.

## Building

```bash
$ cd $GOPATH/src/bitbucket.org/stack-rox/stackrox

$ make -C kernel-modules build-coreos-repackage
```

Produces a single Docker image tagged with `stackrox/coreos-repackage:latest`.

## Usage

### Preconditions

In this example, there is a single input development container file located at `input/coreos_developer_container.bin.bz2`, that will be repackaged.

```bash
$ tree
  .
  └── input
      └── coreos_developer_container.bin.bz2

  1 directory, 1 file

$ file input/coreos_developer_container.bin.bz2
  input/coreos_developer_container.bin.bz2: bzip2 compressed data
```

### Repackaging

We can use the `stackrox/coreos-repackage:latest` image built earlier, to repackage the CoreOS development container file. Due to the size of the input file, this process may take up to ~1 minute.

Note the use of the `--privileged` flag to `docker run`. This is **required** to successfully mount the development container.

```bash
$ docker run --rm -t \
    --privileged \
    -v $PWD/input:/input:ro \
    -v $PWD/output:/output \
    stackrox/coreos-repackage:latest
  Mon Jul 23 22:06:26 UTC 2018 | Repackaging /input/coreos_developer_container.bin.bz2 into /output/bundle.tgz.
  Mon Jul 23 22:06:26 UTC 2018 | Inflating archive
  Mon Jul 23 22:07:05 UTC 2018 | Mounting image
  Mon Jul 23 22:07:05 UTC 2018 | Bundling files
  Mon Jul 23 22:07:20 UTC 2018 | Unmounting image
  Mon Jul 23 22:07:20 UTC 2018 | Cleaning image
```

### Postconditions

After repackaging there is a single gzipped output tarball located at `output/bundle.tgz`, that can be later consumed when building kernel modules.

```bash
$ tree
  .
  ├── input
  │   └── coreos_developer_container.bin.bz2
  └── output
      └── bundle.tgz

  2 directories, 2 files

$ file output/bundle.tgz
  output/bundle.tgz: gzip compressed data, max compression

$ tar -tf output/bundle.tgz
  usr/boot/config
  lib/modules/
  lib/modules/4.11.6-coreos/
  lib/modules/4.11.6-coreos/modules.builtin.bin
  lib/modules/4.11.6-coreos/modules.softdep
  lib/modules/4.11.6-coreos/modules.dep.bin
  <snip...>
  lib/modules/4.11.6-coreos/source/scripts/checksyscalls.sh
  lib/modules/4.11.6-coreos/source/scripts/cleanfile
  lib/modules/4.11.6-coreos/source/scripts/decodecode
```

#### Uploading to GCS

In order for this bundle to be used in future CI builds, you must upload it to
Google Cloud Storage.

To find the path, run `make print-package-cache-path` in the parent directory
(`kernel-modules`).

Then, construct your output path by taking the input URL, changing the
filename to `bundle.tgz`, and URL-encoding the URL. For instance,
`http://stable.release.core-os.net/amd64-usr/1688.5.3/coreos_developer_container.bin.bz2`
becomes
`gs://stackrox-kernel-headers-mirror/packages/http:%2F%2Fstable.release.core-os.net%2Famd64-usr%2F1688.5.3%2Fbundle.tgz`

Then, upload: `gsutil cp output/bundle.tgz FINAL_FILENAME`.
