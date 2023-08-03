FROM quay.io/centos/centos:stream9 AS base

RUN dnf install -y \
    bash \
    bc \
    bison \
    curl-minimal \
    diffutils \
    elfutils-devel \
    flex \
    kernel-devel \
    make \
    openssl-devel

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
# Bcc image that contains bcc-tools
#
FROM base AS bcc

RUN dnf install -y bcc-tools && \
    ln -s /usr/bin/python3 /usr/bin/python

ENV PATH=/bin:/usr/bin:/usr/share/bcc/tools:/tools
ENTRYPOINT ["/scripts/run-tool.sh"]

#
# Perf image that just contains the perf tool
#
FROM base AS perf

RUN dnf install -y perf

ENTRYPOINT ["/scripts/run-tool.sh", "perf"]
