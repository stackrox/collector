FROM pipeline:src

ENV CHECKOUT_BEFORE_PATCHING=true
ENV DOCKERIZED=1
ENV USE_KERNELS_FILE=true
ENV OSCI_RUN=1
ENV MAX_PARALLEL_BUILDS=32

COPY --from=replaced-by-osci:scripts /scripts/ /scripts/

USER root

RUN ln -s /go/src/github.com/stackrox/collector/ /collector && \
    git -C /collector fetch --all && \
    . /scripts/pr-checks.sh && \
    /scripts/patch-files.sh $BRANCH $LEGACY_PROBES && \
    mkdir /kernels && \
    cp /collector/kernel-modules/KERNEL_VERSIONS /kernels/all && \
    /scripts/process-kernels.sh && \
    /scripts/kernel-splitter.py
