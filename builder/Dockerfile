FROM ubuntu:focal

ARG NPROCS=6
ENV NPROCS=$NPROCS

ENV DEBIAN_FRONTEND=noninteractive

# Install packaged dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        binutils \
        build-essential \
        ca-certificates \
        clang \
        cmake \
        curl \
        g++ \
        gcc \
        gdb \
        git \
        libcap-ng-dev \
        libcurl4-openssl-dev \
        libelf-dev \
        libssl-dev \
        libtool \
        libz-dev \
        pkg-config \
        python \
        rsync \
        ssh \
        tar \
        unzip \
        uuid-dev \
        wget \
    && apt-get clean \
    ;

# We want to fail if the destination directory is there, hence mkdir (not -p).
RUN mkdir install-tmp

COPY install/*.sh install-tmp/

# Build dependencies from source
RUN ./install-tmp/install.sh

RUN ldconfig

# Copy Makefile for generating protobuf code.
COPY build/protogen.mk /

# Copy script for building collector and sysdig
COPY build/build-collector.sh /
RUN chmod 700 /build-collector.sh

# Set up ssh for remote development with IDE
RUN ( \
    echo 'LogLevel DEBUG2'; \
    echo 'PermitRootLogin yes'; \
    echo 'PasswordAuthentication yes'; \
    echo 'HostKey /etc/sshkeys/ssh_host_ed25519_key'; \
    echo 'HostKeyAlgorithms ssh-ed25519'; \
    echo 'Subsystem sftp /usr/lib/openssh/sftp-server'; \
  ) > /etc/ssh/sshd_config_remote_development \
  && mkdir /run/sshd

# Add remote development user
RUN useradd -m remoteuser \
  && yes c0llectah | passwd remoteuser

# Create directory to copy collector and sysdig source into builder container
RUN mkdir /src && chmod a+rwx /src

CMD ["/usr/sbin/sshd", "-D", "-e", "-f", "/etc/ssh/sshd_config_remote_development"]

