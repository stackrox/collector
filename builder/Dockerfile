FROM quay.io/centos/centos:stream8

ARG NPROCS=6
ENV NPROCS=$NPROCS

COPY . /go/src/github.com/stackrox/collector
WORKDIR /go/src/github.com/stackrox/collector

USER root

RUN ./builder/build-builder.sh

CMD ["/usr/sbin/sshd", "-D", "-e", "-f", "/etc/ssh/sshd_config_remote_development"]
