# Stackrox Collector

![GitHub branch checks state](https://img.shields.io/github/checks-status/stackrox/collector/master?style=for-the-badge)
![GitHub](https://img.shields.io/github/license/stackrox/collector?style=for-the-badge)
![GitHub tag (latest SemVer)](https://img.shields.io/github/v/tag/stackrox/collector?sort=semver&style=for-the-badge)

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

3. [Roadmap](docs/roadmap.md): Our plans and directions for future development.

3. [Release Process](docs/release.md): Having troubles with the release? Here
   we have a few tips for you.

4. [References](docs/references.md): Contains a comprehensive list of
   configuration options for the project.
