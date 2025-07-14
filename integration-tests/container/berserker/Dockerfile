FROM  quay.io/rhacs-eng/qa:berserker-1.0-79-g617ec32386

RUN dnf install -y which netcat iproute bpftool procps iptables

COPY workloads/ /etc/berserker/
COPY prepare-tap.sh /scripts/

ENV PATH="${PATH}:/usr/local/bin"

ENTRYPOINT ["/usr/local/bin/berserker"]
