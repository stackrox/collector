ARG collector_version=xxx
ARG collector_repo=quay.io/rhacs-eng/collector
ARG base_image=${collector_repo}:${collector_version}-slim

FROM python:3 as kernel-modules
ARG max_layer_size=xxx
ARG required_layers=xxx

COPY kernel-modules/ /kernel-modules
COPY partition-probes.py /

WORKDIR /kernel-modules

# Split probes into directories containing at most max_layer_size bytes.
# passing 0 as max_layer_depth because we have pre-calculated and pre-validated
# the outcome
RUN /partition-probes.py "0" "${max_layer_size}" "." "/layers" && \
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

FROM probe-layer-8 AS probe-layer-9
COPY --from=kernel-modules /kernel-modules/8 /kernel-modules/
