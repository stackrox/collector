FROM quay.io/centos/centos:stream9

RUN yum install -y yum-utils && \
    yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo && \
    yum install -y docker-ce \
                   docker-ce-cli

ADD stats.sh /

ENTRYPOINT ["/stats.sh"]
