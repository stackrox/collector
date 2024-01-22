FROM quay.io/centos/centos:stream9 as base

RUN yum install -y yum-utils && \
    yum clean all

FROM base AS base-s390x

RUN yum-config-manager --add-repo https://download.docker.com/linux/rhel/docker-ce.repo

FROM base AS base-ppc64le

RUN yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo

FROM base AS base-amd64

RUN yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo

FROM base AS base-arm64

RUN yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo

FROM base-${TARGETARCH} as final

RUN yum install -y docker-ce docker-ce-cli && \
    yum clean all

COPY stats.sh /

ENTRYPOINT ["/stats.sh"]
