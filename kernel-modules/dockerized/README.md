# Dockerized driver builds explained
The purpose of this entry is to shed some light on what the dockerized driver build is, how it works and provide a detailed explanation behind the logic used.

## What is a “dockerized driver”?
Basically, it is just a kernel module or eBPF probe (from here on referred to as “driver”) that has been built by running a single docker build command. The dockerfile used in this build will install any needed tools, checkout the needed collector branch/es, patch code, create a task file listing all drivers to be built, compile all drivers and dump the generated artifacts into a final clean UBI image. Lets now take a closer look at each of these steps.

## The context
The first thing we will need, as in any other container build process, is a context. In order to build a dockerized driver we will need two things in the context:
- The collector repo
- One or more header bundles for the kernels we want to build drivers for.

An example context directory would look as follows:

```shell
ctx
├── bundles
│   ├── bundle-3.10.0-1160.42.2.el7.x86_64.tgz
│   ├── bundle-3.10.0-514.2.2.el7.x86_64.tgz
│   ├── bundle-4.14.44-coreos-r1.tgz
│   ├── bundle-4.15.0-1098-gcp~16.04.tgz
│   ├── bundle-4.18.0-305.19.1.el8_4.x86_64.tgz
│   ├── bundle-5.10.68-16623.39.6-cos.tgz
│   ├── bundle-5.10.68-16695.0.0-cos.tgz
│   ├── bundle-5.11.0-1020-gcp~20.04.tgz
│   ├── bundle-5.11.0-1021-gcp.tgz
│   ├── bundle-5.14.12-100.fc33.x86_64.tgz
│   ├── bundle-5.15.0-1-cloud-amd64.tgz
│   └── bundle-5.4.0-1055-gcp.tgz
└── collector
```

Notice that you could have as many bundles as you want, the build process will cycle through them and build all possible drivers.

## The base stages
Now that we have the context used for the build process in place, we can start looking at the different stages used in the Dockerfiles involved.

We will start with the "base" stages, their purpose is to provide the tooling needed by other stages down the pipeline.

### rhel-7-base
This stage is based in ubi7 and provides all tooling needed for v3 kernels to be built.

### rhel-8-base
This stage is based in ubi8, provides all tooling needed for v4 and newer kernels to be built and some additional tools needed by other stages.

### Build arguments
- `REDHAT_USERNAME`: A Red Hat developer account username, used to register the image.
- `REDHAT_PASSWORD`: The password correspondig to the Red Hat developer account username provided, used to register the image.

Both base images also copy `/collector/kernel-modules/dockerized/scripts` in a directory separate, this is done to prevent scripts from being modified when running the `patcher` stage (more on this in the following section).

## The patcher stage
Once we have our base images, we need to determine what versions of collector we actually want to build. As a bare minimum, the `patcher` stage will try to apply patches for the specified branch and, if configured to build legacy probes, it will go through all published versions of collector and apply any patches, creating separate source directories for each of them.

### Build arguments
- `BRANCH`: Specifies the main branch being used to build the drivers, defaults to `master`.
- `LEGACY_PROBES`: Defaults to `false`
  - `true`: Build drivers for all currently supported versions of collector.
  - `false`: Build drivers for the specified `BRANCH` only.

For further details, all logic used by this step can be found under `/collector/kernel-modules/dockerized/scripts/patch-files.sh`, in short a set of directories will be created under `/kobuild-tmp/versions-src` in the image, holding all required sources needed to build the drivers. Said sources are later used by the `task-master` and `builders` stages.

## Caching stage
This is a one line stage, meant to provide drivers from a previous run in order to prevent them from being re-built.

### Build arguments
- `CACHE_REPO`: Specifies the repo to pull the cache from, currently only support `quay.io/rhacs-eng`
- `CACHE_TAG`: The tag to be used as cache.

Any drivers that exist in the cache stage will prevent newer versions of the same driver from being built.

## Task master stage
This stage is responsible for creating a task file, detailing what combinations of kernels, collector version and driver flavor (kernel module or eBPF) will be built.

### Build arguments
- `USE_KERNELS_FILE`: Defaults to `false`.
  - `true`: Build the list of tasks using all kernels listed in `/collector/kernel-modules/KERNEL_VERSIONS`, disregarding what bundles are available under `/bundles`.
  - `false`: Build the list of tasks using the kernel bundles available.

The purpose of building the list of tasks using the `KERNEL_VERSIONS` file is for our CI to be able to extract it from a built container and pull any bundles needed from GCP, instead of downloading the complete cache of bundles for every build. If the list is being created from the available bundles, it will be assumed that all possible drivers for all existing bundles are being built.

For further details, all logic used by this step can be found under `/collector/kernel-modules/dockerized/scripts/get-build-tasks.sh`.

## RHEL 7/8 Builders
The builder stages are responsible for building all drivers listed in the `/build-tasks` created by the `task-master` stage. The RHEL 7 builder handles kernels v3 and below, the RHEL 8 builder handles v4 and up.

# Merging multiple builds
In order to speed up the build process of dockerized drivers, multiple images are built by CI in parallel and pushed to `quay.io`. In order to unify all these images into a final image, `Dockerfile.merge` takes a repository (`--build-arg DRIVERS_REPO`), a base tag (`--build-arg BASE_TAG`) that is used to accumulate drivers, and a second tag (`--build-arg EXTRA_DRIVERS`) from where to retrieve drivers. Once all drivers have been aggregated, `Dockerfile.squash` can be used to create a final image with only two layers.

## Dockerfile.merge
This Dockerfile simply takes all drivers from the `$DRIVERS_REPO/collector-drivers:$EXTRA_DRIVERS` and copies the over on top of the `$DRIVERS_REPO/collector-drivers:$BASE_TAG` image. The `/FAILURES` directory is also copied in order to preserve build failures.

## Dockerfile.squash
The only purpose of this Dockerfile is to reduce the number of layers in the final image, without this step, using 32 concurrent nodes for build will result in an image with 128 layers, if this image is to be used as the base for a subsequent build the max number of layers will be exceeded.

# Building a full collector image
Once all drivers are merged into a single image, `Dockerfile.collector` can be used to create a full image of collector.

# Building local changes.
In order to include local changes in the kernel object build, ensure that you use `--build-arg CHECKOUT_BEFORE_PATCHING=false` to prevent overwriting your changes.

## Build arguments
- `COLLECTOR_TAG`: The tag used to pull a collector image (normally a `-slim` variant to add all dockerized drivers into).
- `COLLECTOR_REPO`: The repository to pull collector from, defaults to `quay.io/rhacs-eng`.
- `DRIVERS_TAG`: The tag used to pull the drivers image, defaults to `latest` but it is heavily recommended to change it.
- `DRIVERS_REPO`: The repository to pull drivers from, defaults to `quay.io/rhacs-eng`.
- `CHECKOUT_BEFORE_PATCHING`: whether to checkout the repository sources before patching the probe files (if true, this will overwrite local changes)