ARG collector_version=xxx
ARG collector_repo=stackrox/collector
ARG base_image=$collector_repo:${collector_version}-latest-base

ARG module_version=xxx
ARG base_modules_image=stackrox-kernel-modules:${module_version}

FROM ${base_modules_image} AS all_modules
FROM ${base_image} AS base_modules

FROM alpine:3.10 AS diff_modules

RUN apk update && apk add bash

SHELL ["/bin/bash", "-c"]

COPY --from=all_modules /kernel-modules/*.gz /modules/all/
COPY --from=base_modules /kernel-modules/*.gz /modules/base/

RUN sort <(cd /modules/all && sha256sum *.gz) <(cd /modules/base && sha256sum *.gz) \
        | uniq -u | awk '{print$2}' | sort -u >/modules/diff-manifest \
    && cat /modules/diff-manifest \
    && mkdir /modules/diff \
    && ( cd /modules/all && xargs -i </modules/diff-manifest cp {} /modules/diff/ ) \
    && ls -l /modules/diff/

# We need some file in the directory, otherwise the COPY below will fail. Thanks Docker for
# not supporting a copy that allows an empty source list. It's only been an FR since 2015.

RUN touch /modules/diff/MARKER.gz

FROM ${base_image}

COPY --from=diff_modules /modules/diff/*.gz /kernel-modules/
RUN rm -f /kernel-modules/MARKER.gz

