FROM quay.io/centos/centos:stream8

ARG NPROCS=6
ENV NPROCS=$NPROCS
ARG BUILDER_ROOT_DIR=.
ENV BUILDER_ROOT_DIR=$BUILDER_ROOT_DIR

COPY . /go/src/github.com/stackrox/collector
WORKDIR /go/src/github.com/stackrox/collector

USER root

RUN dnf -y update \
    && dnf -y install \
        autoconf \
        automake \
        binutils-devel \
        bison \
        ca-certificates \
        clang \
        cmake \
        cracklib-dicts \
        diffutils \
        elfutils-libelf-devel \
        file \
        flex \
        gcc \
        gcc-c++ \
        gdb \
        gettext \
        git \
        glibc-devel \
        libasan \
        libcap-ng-devel \
        libcurl-devel \
        libtool \
        libuuid-devel \
        make \
        openssh-server \
        openssl-devel \
        patchutils \
        passwd \
        pkgconfig \
        python2 \
        rsync \
        tar \
        unzip \
        valgrind \
        wget \
        which \
    && dnf clean all

# We want to fail if the destination directory is there, hence mkdir (not -p).
RUN mkdir /install-tmp

RUN cp -r "${BUILDER_ROOT_DIR}"/install/*.sh "/install-tmp"

# Build dependencies from source
RUN "/install-tmp/install.sh"

RUN echo '/usr/local/lib' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig

# Set up ssh for remote development with IDE
RUN ssh-keygen -A \
   && ( \
    echo 'LogLevel DEBUG2'; \
    echo 'PermitRootLogin yes'; \
    echo 'PasswordAuthentication yes'; \
    echo 'HostKey /etc/sshkeys/ssh_host_ed25519_key'; \
    echo 'HostKeyAlgorithms ssh-ed25519'; \
    echo 'Subsystem sftp /usr/libexec/openssh/sftp-server'; \
  ) > /etc/ssh/sshd_config_remote_development \
  && mkdir /run/sshd

# Add remote development user
RUN useradd -m remoteuser \
  && yes c0llectah | passwd remoteuser || if [[ $? -eq 141 ]]; then true; else exit $?; fi

# Create directory to copy collector source into builder container
RUN mkdir /src && chmod a+rwx /src

CMD ["/usr/sbin/sshd", "-D", "-e", "-f", "/etc/ssh/sshd_config_remote_development"]
