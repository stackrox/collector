## MODULE_VERSION

The version contained within the MODULE_VERSION file in this directory is
an internal version used by collector to identify compatibility with built kernel
module/eBPF probes.

The version follows [Semantic Versioning](https://semver.org) to provide a logical
definition of each version number.

When changes are made to `kernel-modules/probe/*` or `falcosecurity-libs/driver/*`,
the MODULE_VERSION file must be updated to reflect these changes, following semver to
decide which version number to increase.

For pre-release, the version should be appended with `-rcX` where `X` is a strictly
increasing integer, starting from `1`. Upon release, the release candidate version
will be dropped in favour of the final released version, as detailed in the [release
documentation](../docs/release.md).
