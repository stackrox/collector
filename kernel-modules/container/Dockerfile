ARG collector_version=xxx
ARG collector_repo=stackrox/collector
ARG base_image=$collector_repo:${collector_version}-base

FROM python:3 as kernel-modules
ARG max_layer_size=300000000
ARG max_layer_depth=8

COPY kernel-modules/ /kernel-modules
COPY bucket-probes.py /

# Split probes into directories containing at most max_layer_size bytes.
RUN cd /kernel-modules && \
  /bucket-probes.py "${max_layer_depth}" "${max_layer_size}" "." "/layers" && \
  for i in "/layers"/*; do mkdir -p "$(basename "$i")"; xargs -a "$i" mv -t "$(basename "$i")" ; done

# Probes are split across multiple layers to limit blob size
FROM ${base_image} AS probe-layer-1
COPY --from=kernel-modules /kernel-modules/0 /kernel-modules/

FROM probe-layer-1 AS probe-layer-2
COPY --from=kernel-modules /kernel-modules/1 /kernel-modules/

FROM probe-layer-2 AS probe-layer-3
COPY --from=kernel-modules /kernel-modules/2 /kernel-modules/

FROM probe-layer-3 AS probe-layer-4
COPY --from=kernel-modules /kernel-modules/3 /kernel-modules/

FROM probe-layer-4 AS probe-layer-5
COPY --from=kernel-modules /kernel-modules/4 /kernel-modules/

FROM probe-layer-5 AS probe-layer-6
COPY --from=kernel-modules /kernel-modules/5 /kernel-modules/

FROM probe-layer-6 AS probe-layer-7
COPY --from=kernel-modules /kernel-modules/6 /kernel-modules/

FROM probe-layer-7 AS probe-layer-8
COPY --from=kernel-modules /kernel-modules/7 /kernel-modules/
