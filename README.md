# Collector

## Collector development

### Cloning the repo

Since the collector repo uses a submodule for the sysdig source, use
the following command to clone.

```
git clone --recurse-submodules git@github.com:stackrox/collector.git
```

### Development with an IDE (CLion)

#### Setup
These instructions are for using the *JetBrains* C/C++ IDE **CLion**, but should be adaptable to any IDE that supports development over ssh/sftp.
- If running CLion IDE for the first time:
  - Download and install CLion (https://www.jetbrains.com/clion/download/)
  - Create a project from the `collector/collector` directory using an existing CMakeLists file (`collector/collector/CMakeLists.txt`).
- Create the collector builder container if not already running, or if the builder image has changed.
  - `docker run -d --cap-add sys_ptrace -p127.0.0.1:2222:22 --name collector_remote_dev stackrox/collector-builder:cache`
  - This image can be built locally using `BUILD_BUILDER_IMAGE=true make builder` from `builder/Dockerfile{_rhel}` Dockerfile.
- In the **CLion->Preferences** window, add a new **Toolchain** entry in settings under **Build, Execution, Deployment** as a **Remote Host** type.
- Then click in the **Credentials** section and fill out the SSH credentials used in the builder Dockerfile.
  - Host: `localhost`, Port: `2222`, User name: `remoteuser`, Password: `c0llectah`
- Next, select **Deployment** under **Build, Execution, Deployment**, and then **Mappings**. Change **Deployment path** to `/src`.
- Finally, add a CMake profile that uses the **Remote Host** toolchain and change build directory to `cmake-build`.

The development workflow can also be used with the rhel based builder image. For a CMake profile to use with the Red Hat builder, change the build
directory to `cmake-build-rhel`.

To remove the collector builder container: `docker rm -fv collector_remote_dev`

#### Compilation and Testing
- If you delete and recreate the `collector_remote_dev` container, you may need click **Resync with Remote Hosts** under the **Tools** menu.
- To build the sysdig wrapper libary and collector binary: select the *collector* configuration from the **Run...** menu and then **Build**.
- To run unit tests, select the *runUnitTests* configuration and then select **Run**.

### Building collector image(s) from the command-line
- `make image` or `make image-rhel` will create the default (Ubuntu) and Red Hat based collector images respectively.



