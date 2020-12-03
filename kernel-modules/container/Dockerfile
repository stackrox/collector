ARG collector_version=xxx
ARG collector_repo=stackrox/collector
ARG base_image=$collector_repo:${collector_version}-base
FROM ${base_image}
ARG module_version=xxx
ADD ${module_version}.tar.gz /kernel-modules/
COPY LICENSE /kernel-modules/
