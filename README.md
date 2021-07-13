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

The development workflow can also be used with the rhel based builder image.
  - Start the collector builder with `make start-dev-rhel`
  - Create a Red Hat CMake profile with **Build Directory**/**Generation Path** set to `cmake-build-rhel`.

#### Teardown
- Run `make teardown-dev` to remove the builder container and associated ephemeral ssh keys from `$HOME/.ssh/known_hosts`
- After restarting, you may need click **Resync with Remote Hosts** under the **Tools** menu in **CLion**.

#### Compilation and Testing
- To build the sysdig wrapper libary and collector binary: select the *collector* configuration from the **Run...** menu and then **Build**.
- To run unit tests, select the *runUnitTests* configuration and then select **Run**.

### Building collector image(s) from the command-line
- `make image` or `make image-rhel` will create the default (Ubuntu) and Red Hat based collector images respectively.



