# Kernel Modules build

This image exists to pipe kernel modules and related libraries
into the Collector build. It is the base image of the Collector.
This image contains Sysdig kernel module.

## How does the build work?
We want to discover all the kernels we need to build for, then build
the relevant modules.

There are two steps to getting there:

 1. Crawl for available kernels from supported distributions
 1. Build modules for each of the discovered kernels.

## How does a collector get the modules we build here?
There are three ways a collector can get the relevant kernel module:

1. It is built into the collector, since this image is the base image
for `collector`.
1. Failing that, collector will attempt to download it from a path under
https://collector-modules.stackrox.io. This path is specified in this build
and referenced in Director; it is intended to protect against URL guessing
but nothing else. This service is configured by the `stackrox.io` deployment
and is backed by the Google Cloud Storage (GCS) bucket that
`make upload-modules` targets.
1. Failing that, for instance when the cluster has no Internet connectivity,
we can manually build the module and distribute it to the customer out-of-band.
Then, the customer can mount it into the container using a volume mount.

## How to build
`make all` will do everything.

### A note on the package cache
`make download-package-cache` will pull all previously-downloaded kernel
packages from a Google Cloud Storage (GCS) bucket.
This makes builds more reliable compared with the alternative of downloading
each URL individually from a distribution's mirror, and also allows us to build
files that are not publicly available (e.g., Red Hat packages).

`make upload-package-cache` will `gsutil rsync` your local cache to GCS.
This is done in CI so that future builds can get the new packages in one shot.

`make delete-package-cache` deletes your local copy of the packages.
`make clean` does not execute this step because of the time it takes to
re-download the entire cache.

## How to add supported kernels
Run `make crawl`.

Generally, you can simply commit the resulting files.
If URLs are removed, check whether there are errors in the script output.
Generally, lines should not be removed except for the ELRepo kernel versions
discussed below.

There is one exception: Kernels from [ELRepo](https://elrepo.org) need to be
moved from the `supported-kernels/centos.txt` list to
`supported-kernels/centos-uncrawled.txt` instead of deleted. This is because
the ELRepo mirrors (even archive mirrors) only keep 25 recent versions,
but customers run older ones than that. (Running `make crawl` will automatically
do this, but verify before merging that it did the right thing.)

Submit a pull request with the modifications and commit it.

## How to "backport" new kernels to old releases
There is not a very well automated process for this, unfortunately.

Generally what you should do is:

1. Check out the release branch. (Otherwise you will build with new code,
which may not be compatible.)
1. `sudo rm -rf container/kernel-modules/ sysdig sysblock` (to delete old artifacts)
1. Change `supported-kernels/*.txt` to only have the added lines from
the time since the release happened. Any files that do not have any edits
should be changed to have a single valid URL line (due to build difficulties
with empty files).
This list can be ascertained using a command like:
```bash
git diff HEAD develop -- kernel-modules/supported-kernels/ | grep + | grep -v '@' | cut -c 2-
```
1. Run `make all upload-modules`. Note that your Google Cloud identity must
have permissions in the stackrox-collector project to do this. Consult a cloud
admin if you need permissions and do not have them.

An alternative is to do a full CI build and then use the modules it built:

1. Wait for the build to complete.
1. Run a kernel-modules container and `docker cp` the entire `/kernel-modules`
directory out from it to your local host `container/kernel-modules`.
1. Make sure `ROX_VERSION` is set correctly -- the easiest way to ensure this
is to check out the release branch.
1. Run `make upload-modules` to upload all the modules to
collector-modules.stackrox.io. Note that your Google Cloud identity must
have permissions in the stackrox-collector project to do this. Consult a cloud
admin if you need permissions and do not have them.

## How to add a new target distro
Follow the example of the existing distros. You'll need to find a website
(the more official the better) where you can HTML-scrape links to package
downloads.

If the OS you're targeting is supported by Sysdig, it is worth adapting
their [build-probe-binaries](https://github.com/draios/sysdig/blob/dev/scripts/build-probe-binaries)
script function and adding it to `build/build-kos`.

Then, add the requisite targets to the Makefile and commit the list of kernel
package URLs.
