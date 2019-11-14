ARG collector_version=xxx
ARG collector_repo=stackrox/collector
ARG base_image=$collector_repo:${collector_version}-base

ARG module_version=xxx
ARG base_modules_image=stackrox-kernel-modules:${module_version}

FROM ${base_modules_image} AS modules

FROM ${base_image}

COPY --from=modules /kernel-modules/*.gz /kernel-modules/
