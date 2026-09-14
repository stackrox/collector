FROM quay.io/stackrox-io/berserker:network-1.0-85-g1b7ab034aa

COPY workloads/ /etc/berserker/
COPY network/workloads/ /etc/berserker/

ENV PATH="${PATH}:/usr/local/bin"

ENTRYPOINT ["/usr/local/bin/berserker"]
