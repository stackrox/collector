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
$ make image
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
*NOTE*: To build a multi-arch image, use the `ma-image` target instead which
uses `docker buildx` to build and push the image to registry in a single step.


## Run inside a container

To experiment with the freshly built Collector you need to run the
corresponding container together with a mock gRPC server to actually see it in
action. One easy way to achieve this is to use docker compose with the
following configuration:

```yaml
version: "3.9"
services:
  collector:
    image: quay.io/stackrox-io/collector:<debuggable-collector-tag>
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

### Development with an IDE (CLion)

#### Setup
These instructions are for using the *JetBrains* C/C++ IDE **CLion**, but should be adaptable to any IDE that supports development over ssh/sftp.
- If running CLion IDE for the first time:
  - Download and install CLion (https://www.jetbrains.com/clion/download/)
  - Create a project from the `collector/collector` directory using an existing CMakeLists file (`collector/collector/CMakeLists.txt`).
- Create the collector builder container if not already running, or if the builder image has changed.
  - `make start-dev`
    - (Optional) Local builder images can used by setting the environment variable before execution using `BUILD_BUILDER_IMAGE=true make start-dev`.
Or, builder images from a PR by with `COLLECTOR_BUILDER_TAG=<circle-build-id> make start-dev`.

Instructions for Mac OS
- In the **CLion->Preferences** window, add a new **Toolchain** entry in settings under **Build, Execution, Deployment** as a **Remote Host** type.
- Then, click in the **Credentials** section and fill out the SSH credentials used in the builder Dockerfile.
  - Host: `localhost`, Port: `2222`, User name: `remoteuser`, Password: `c0llectah`
- Next, select **Deployment** under **Build, Execution, Deployment**, and then **Mappings**. Set **Deployment path** to `/tmp`.
- Finally, add a CMake profile that uses the **Remote Host** toolchain and change **Build directory**/**Generation Path** to `cmake-build`.

Instructions for Linux
- In the **File->Settings->Build, Execution, Deployment->Toolchains** window, add a new **Toolchain** entry as a **Remote Host** type.
- Then, click in the **Credentials** section and fill out the SSH credentials used in the builder Dockerfile.
  - Host: `localhost`, Port: `2222`, User name: `remoteuser`, Password: `c0llectah`
- In the **File->Setting->Build, Execution, Deployment->Deployment** window click on the **Mappings** tab. Set **Deployment path** to /tmp.
- In the **File->Settings->Build, Execution, Deployment->CMake** window add a CMake profile that uses the **Remote Host** toolchain and change **Build directory**/**Generation Path** to `cmake-build`.

#### Teardown
- Run `make teardown-dev` to remove the builder container and associated ephemeral ssh keys from `$HOME/.ssh/known_hosts`
- After restarting, you may need click **Resync with Remote Hosts** under the **Tools** menu in **CLion**.

#### Compilation and Testing
- To build the Falco wrapper libary and collector binary: select the *collector* configuration from the **Run...** menu and then **Build**.
- To run unit tests, select the *runUnitTests* configuration and then select **Run**.

### Development with Visual Studio Code
#### Setup for C++ using devcontainers
Visual Studio Code can be used as a development environment by leveraging its devcontainers feature.
- Install the [remote-containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension.
- Create a `.devcontainer.json` file under `collector` and set the `image` attribute to `quay.io/stackrox-io/collector-builder:cache`
```json
{
  "name": "collector-dev",
  "image": "quay.io/stackrox-io/collector-builder:cache"
}
```
- Open the `collector/` directory in a new instance of Visual Studio Code and when prompted select **Reopen In Container**, a new container should be created and the workspace directory will be mounted inside of it.

#### Teardown
Closing the Visual Studio Code instance will stop the container.

#### Important note on performance
Even though development containers is a supported feature of `Docker for Desktop`, there is a [bug](https://github.com/docker/for-mac/issues/3677) that tanks performance when running containers with mounted volumes. A possible workaround is to setup docker to run inside a VM and mounting the work directories using NFS.

### Building collector image(s) from the command-line
- `make image` will create the Red Hat based collector image.
- `make image-dev` will create a collector image based on CentOS Stream, the package manager in this image is available so additional developer tools can be installed.
- `make image-dev-full` will extend the `image-dev` target and also compile and embed the kernel module and eBPF probes in the final image, making it easier to test changes in these components.

### Setting up git hooks

Git hooks are configured in the `.pre-commit-config.yaml` file using [pre-commit](https://pre-commit.com)
In order to use them install pre-commit, and run `git config core.hooksPath ./githooks/` from the collector root directory.

## Run in Minikube
### Build the collector image
Minikube comes with a pre-bundled docker engine that can be used to directly build the collector image and make it available to the cluster.
If you are using the hyperkit driver, some amount of tinkering will be required for the collector repository to be shared with the underlying
VM and it to be usable for building the image. You can use `nfs` to mount the repository, then use a symlink to adjust absolute paths:
```sh
$ minikube start --nfs-share='/path/to/collector'
$ minikube ssh
$ sudo ln -s /nfsshare/path/to/collector /path/to/collector
```

Then you can configure your host to use the bundled docker and build the image with the following commands:
```sh
$ eval $(minikube -p minikube docker-env)
$ make image
```

## Debugging
### Debugging on a k8s deployment
First step for debugging collector on kubernetes is to have a debug enabled collector image with `gdb` installed.
You can build such an image by running the following command at the root of the repository:
```sh
CMAKE_BUILD_TYPE=Debug make image-dev
```

You will also need a full deployment of Stackrox up and running. You can get precise instructions on how to deploy it
[here](https://github.com/stackrox/stackrox/blob/master/README.md#deploying-stackrox).

With the image built and Stackrox up and running, you can make the following changes to the collector daemonset by
running `kubectl -n stackrox edit ds/collector`

```yaml
# ...
spec:
  template:
    spec:
      containers:
      - env:
        # Make sure this variable is edited like this for proper JSON escaping
        - name: COLLECTOR_CONFIG
          value: |
            '{"tlsConfig":{"caCertPath":"/var/run/secrets/stackrox.io/certs/ca.pem","clientCertPath":"/var/run/secrets/stackrox.io/certs/cert.pem","clientKeyPath":"/var/run/secrets/stackrox.io/certs/key.pem"}}'
        # This will direct collector to run under GDB
        - name: COLLECTOR_PRE_ARGUMENTS
          value: gdbserver 0.0.0.0:1337
        image: quay.io/stackrox-io/collector:<your-built-tag-here>
        # Expose the port GDB will be listening on
        ports:
        - containerPort: 1337
          name: gdb
          protocol: TCP
```

Once the configuration is applied, the collector pod will restart and wait for a GDB client to connect to it.
In order to connect to collector, you can run the following two commands:
```sh
# Make the GDB port reachable from your host
kubectl -n stackrox port-forward ds/collector 40000:1337 &
# Connect to the remote GDB server and map collector symbols to your sources
gdb -ex "target extended-remote localhost:40000"\
    -ex "set substitute-path /tmp/collector /path/to/stackrox/collector"
```

You should now be attached to a debug session. If you need to restart the session, you will need to recreate
both the collector pod and the port-forwarding process.

## Profiling
