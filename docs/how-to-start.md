# How to start

## Build locally

To build Collector locally the first step is of course to fetch the repository
with all the submodules:

```bash
$ git clone --recurse-submodules https://github.com/stackrox/collector.git
```

Inside the project you can find the `Makefile` containing most of the targets
needed for a quick start. To build an image with Collector use target `image`:

```bash
$ cd collector
$ CMAKE_BUILD_TYPE=Debug make image
```

This target will build necessary submodules (gRPC dependencies, Falco
libraries), prepare a builder image, compile Collector using it, and wrap
everything into a slim image with Collector binary inside.

*NOTE*: Using an intermediate image for compilation means that file paths are
going to be different between your local project directory and the image. For
convenience you may want to map paths, e.g. for VSCode add following into the
`launch.json`:

```json
"sourceFileMap":{
    "/src": "/Path/To/collector/collector"
},
```

Or pipe output of the make target:

```bash
$ make image 2>&1 | sed -e "s|/src|${PWD}/collector/|g"
```

## Run inside a container

To experiment with the freshly built Collector you need to run the
corresponding container together with a mock gRPC server to actually see it in
action. One easy way to achieve this is to use docker compose with the
following configuration:

```yaml
version: "3.9"
services:
  collector:
    image: stackrox/collector:<debuggable-collector-tag>
    container_name: collector-debug
    network_mode: host
    privileged: true
    environment:
      - GRPC_SERVER=localhost:9999
      - COLLECTOR_CONFIG={"logLevel":"debug","turnOffScrape":true,"scrapeInterval":2}
      - COLLECTION_METHOD=ebpf
      - MODULE_DOWNLOAD_BASE_URL=https://collector-modules.stackrox.io/612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656
      - MODULE_VERSION=b6745d795b8497aaf387843dc8aa07463c944d3ad67288389b754daaebea4b62
      - COLLECTOR_HOST_ROOT=/host
    volumes:
      - /var/run/docker.sock:/host/var/run/docker.sock:ro
      - /proc:/host/proc:ro
      - /etc:/host/etc:ro
      - /usr/lib:/host/usr/lib:ro
      - /sys/:/host/sys/:ro
      - /dev:/host/dev:ro
    depends_on:
      - grpc-server-debug
  grpc-server-debug:
    image: stackrox/grpc-server:latest
    container_name: grpc-server
    network_mode: host
    user: 1000:1000
    volumes:
      - /tmp:/tmp:rw
```

Using this configuration docker compose will spin up the Collector attached to
the host network in privileged mode. Collector in turn will try to download
probes for your version of Linux kernel and start listening to events happening
inside the container.

## Run in Minikube

## Debugging

## Profiling
