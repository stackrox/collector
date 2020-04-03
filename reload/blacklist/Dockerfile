ARG collector_version=xxx
ARG collector_repo=stackrox/collector
ARG base_image=$collector_repo:${collector_version}-latest

FROM ${base_image}

RUN \
  rm -f kernel-modules/collector-ebpf-4.19.*-coreos*.o.gz && \
  rm -f kernel-modules/collector-ebpf-4.19.*-cos.o.gz \
  ;
