FROM alpine:3.17 AS base

RUN apk add --update \
    bash \
    bc \
    build-base \
    bison \
    flex \
    curl \
    elfutils-dev \
    linux-headers \
    make \
    openssl-dev \
    diffutils

COPY tools /tools
COPY scripts /scripts

WORKDIR /root

#
# Init image for downloading and installing kernel headers
#
FROM base AS init

WORKDIR /
CMD ["/scripts/init.sh"]

#
# BCC image that contains bcc-tools
# see: https://github.com/iovisor/bcc
#
FROM base AS bcc

RUN apk add --update bcc-tools && \
    ln -s /usr/bin/python3 /usr/bin/python

ENV PATH=/bin:/usr/bin:/usr/share/bcc/tools:/tools
ENTRYPOINT ["/scripts/run-tool.sh"]

#
# Perf image that just contains the perf tool
#
FROM base AS perf

RUN apk add --update perf

ENTRYPOINT ["/scripts/run-tool.sh", "perf"]
