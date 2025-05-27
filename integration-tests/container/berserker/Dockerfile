FROM  quay.io/rhacs-eng/qa:berserker-1.0-79-g617ec32386

RUN sudo dnf install -y which iproute bpftool procps iptables

COPY workloads/ /etc/berserker/
COPY init.sh /scripts/
COPY prepare-tap.sh /scripts/

ENV PATH="${PATH}:/usr/local/bin"

ENTRYPOINT ["/usr/local/bin/berserker"]
