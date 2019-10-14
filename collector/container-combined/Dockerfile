ARG collector_version=xxx
ARG base_image=stackrox/collector:${collector_version}-base

ARG module_version=xxx
ARG base_modules_image=stackrox-kernel-modules:${module_version}

FROM ${base_modules_image} AS modules

FROM ${base_image}

COPY --from=modules /kernel-modules/*.gz /kernel-modules/
