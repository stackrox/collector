# Stackrox Collector

| branch | upstream | downstream |    |
| ------ | -------- | ---------- | -- |
| master | [![nighly upstream](https://github.com/stackrox/collector/actions/workflows/main.yml/badge.svg?event=schedule)](https://github.com/stackrox/collector/actions/workflows/main.yml?query=event%3Aschedule++) | [![last downstream](https://github.com/stackrox/collector/actions/workflows/konflux.yml/badge.svg?event=push)](https://github.com/stackrox/collector/actions/workflows/konflux.yml?query=event%3Apush++) | |
| [3.24](https://github.com/stackrox/collector/tree/release-3.24) | [![3.24 upstream](https://github.com/stackrox/collector/actions/workflows/main.yml/badge.svg?branch=release-3.24&event=push)](https://github.com/stackrox/collector/actions/workflows/main.yml?query=event%3Apush+branch%3Arelease-3.24++) | [![3.24 downstream](https://github.com/stackrox/collector/actions/workflows/konflux.yml/badge.svg?branch=release-3.24&event=push)](https://github.com/stackrox/collector/actions/workflows/konflux.yml?query=event%3Apush+branch%3Arelease-3.24++) | [⬆️4.10](https://github.com/stackrox/stackrox/tree/release-4.10) |
| [3.23](https://github.com/stackrox/collector/tree/release-3.23) | [![3.23 upstream](https://github.com/stackrox/collector/actions/workflows/main.yml/badge.svg?branch=release-3.23&event=push)](https://github.com/stackrox/collector/actions/workflows/main.yml?query=event%3Apush+branch%3Arelease-3.23++) | [![3.23 downstream](https://github.com/stackrox/collector/actions/workflows/konflux.yml/badge.svg?branch=release-3.23&event=push)](https://github.com/stackrox/collector/actions/workflows/konflux.yml?query=event%3Apush+branch%3Arelease-3.23++) | [⬆️4.9](https://github.com/stackrox/stackrox/tree/release-4.9) |
| [3.22](https://github.com/stackrox/collector/tree/release-3.22) | [![3.22 upstream](https://github.com/stackrox/collector/actions/workflows/main.yml/badge.svg?branch=release-3.22&event=push)](https://github.com/stackrox/collector/actions/workflows/main.yml?query=event%3Apush+branch%3Arelease-3.22++) | [![3.22 downstream](https://github.com/stackrox/collector/actions/workflows/konflux.yml/badge.svg?branch=release-3.22&event=push)](https://github.com/stackrox/collector/actions/workflows/konflux.yml?query=event%3Apush+branch%3Arelease-3.22++) | [⬆️4.8](https://github.com/stackrox/stackrox/tree/release-4.8) |

Welcome to the Stackrox Collector project documentation. Here you can learn
more about idea behind the project, how to start guidelines, design overview
and detailed references.

Collector is a component of Stackrox responsible for gathering runtime data. In
a few words it is an agent that runs on every node under strict performance
limitations and gathers the data via kernel modules or eBPF probes (the default
collection mode nowadays). To implement eBPF probes and collect data, the
project leverages the Falco libraries via a custom
[fork](https://github.com/stackrox/falcosecurity-libs).

## Useful links

Here are few links to get more details:

1. [How to start](docs/how-to-start.md): If you want to contribute to the
   project, this is the best place to start. This section covers building and
   troubleshooting the project from scratch.

2. [Design overview](docs/design-overview.md): When your goal is to better
   understand how Collector works, and it's place in the grand scheme of
   things, you may want to look here.

3. [Troubleshooting](docs/troubleshooting.md): For common startup errors,
   ways of identifying and fixing them.

4. [Release Process](docs/release.md): Having troubles with the release? Here
   we have a few tips for you.

5. [References](docs/references.md): Contains a comprehensive list of
   configuration options for the project.
