FROM pipeline:src

ENV OSCI_RUN=1

COPY --from=replaced-by-osci:scripts /scripts/ /scripts/

COPY --from=replaced-by-osci:cpaas-drivers-base /FAILURES/ /FAILURES/
COPY --from=replaced-by-osci:cpaas-drivers-base /kernel-modules/ /kernel-modules/
COPY kernel-modules/support-packages/04-create-support-packages.sh /scripts/create-support-packages.sh
COPY collector/LICENSE-kernel-modules.txt /LICENSE
