FROM  quay.io/rhacs-eng/qa:berserker-1.0-59-g87ad0d870e

COPY workloads/ /etc/berserker/

ENV PATH="${PATH}:/usr/local/bin"

ENTRYPOINT ["/usr/local/bin/berserker"]
