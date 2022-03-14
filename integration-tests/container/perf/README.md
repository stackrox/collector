## Performance Measurement Tooling via Docker

The Dockerfiles listed here provide reusable images for performance testing and 
measurement. They are used by the benchmark tests to measure whilst collector is
under load.

The images all contain copies of `scripts/` and `tools/` (mounted at `/scripts` and
`/tools` respectively,) and are otherwise empty alpine images except for those
performance tools they are made for.

The special `init` image will set up the host machine to make it usable by the
tool images. This includes downloading the correct kernel headers and ensuring
`kallsyms` access.

## Building & Pushing

```bash
# build all images
$ make all

# push all images
$ make push-all
```

## Manually Running the Images

```bash
# running the init container
$ docker run --rm \
    -v /lib/modules:/host/lib/modules \
    -v /etc/os-release:/host/etc/os-release \
    -v /etc/lsb-release:/host/etc/lsb-release \
    -v /usr/src:/host/usr/src \
    stackrox/collector-performance:init

# running one of the tools containers
$ docker run --rm --privileged \
    -v /sys:/sys \
    -v /usr/src:/usr/src \
    -v /lib/modules:/lib/modules \
    -v /tmp:/tmp \
    stackrox/collector-performance:<tool> <tool args>
```