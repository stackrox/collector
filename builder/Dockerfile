FROM quay.io/centos/centos:stream8

ARG NPROCS=6
ENV NPROCS=$NPROCS
ARG ROOT=.
ENV ROOT=$ROOT

COPY . /go/src/github.com/stackrox/collector
WORKDIR /go/src/github.com/stackrox/collector

USER root

RUN ./build-builder.sh .

CMD ["/usr/sbin/sshd", "-D", "-e", "-f", "/etc/ssh/sshd_config_remote_development"]
