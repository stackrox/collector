FROM pipeline:src

ENV OSCI_RUN=1

COPY --from=replaced-by-osci:pre-build /kernel-modules /kernel-modules
COPY --from=replaced-by-osci:fc36-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:fc36-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel8-0-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel8-0-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel8-1-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel8-1-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel8-2-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel8-2-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel8-3-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel8-3-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel7-0-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel7-0-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel7-1-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel7-1-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel7-2-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel7-2-drivers /FAILURES /FAILURES
COPY --from=replaced-by-osci:rhel7-3-drivers /built-drivers /built-drivers
COPY --from=replaced-by-osci:rhel7-3-drivers /FAILURES /FAILURES

COPY --from=replaced-by-osci:scripts /scripts/ /scripts/
