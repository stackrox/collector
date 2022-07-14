FROM pipeline:src

COPY scripts /scripts

RUN dnf -y install make && \
    /scripts/gcloud-sdk-install.sh

