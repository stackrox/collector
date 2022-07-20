FROM pipeline:src

ENV OSCI_RUN=1

COPY --from=pre-build:latest /kernel-modules /kernel-modules
COPY --from=fc36-drivers:latest /kernel-modules /kernel-modules
COPY --from=fc36-drivers:latest /FAILURES /FAILURES
COPY --from=rhel8-drivers:latest /kernel-modules /kernel-modules
COPY --from=rhel8-drivers:latest /FAILURES /FAILURES
COPY --from=rhel7-drivers:latest /kernel-modules /kernel-modules
COPY --from=rhel7-drivers:latest /FAILURES /FAILURES

COPY --from=scripts:latest /scripts/ /scripts/
