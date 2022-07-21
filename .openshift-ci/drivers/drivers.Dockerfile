FROM pipeline:src

ENV OSCI_RUN=1

COPY --from=replaced-by-osci:pre-build /kernel-modules /kernel-modules
COPY --from=replaced-by-osci:fc36-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:fc36-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel8-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel8-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel7-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel7-drivers /FAILURES /FAILURES

COPY --from=replaced-by-osci:scripts /scripts/ /scripts/
