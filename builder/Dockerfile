FROM quay.io/centos/centos:stream9

ARG BUILD_DIR=/install-tmp

USER root

# clang 15 is used here due to an error on clang 16 when running on QEMU
# emulating s390x on a x86 host (which is our case on GHA), see:
# - https://bugzilla.redhat.com/show_bug.cgi?id=2209635
# - https://bugzilla.redhat.com/show_bug.cgi?id=2211472
RUN dnf install -y dnf-plugins-core && \
    dnf config-manager --set-enabled crb && \
    dnf install -y epel-release && \
    dnf -y update \
    && dnf -y install --nobest \
        autoconf \
        automake \
        binutils-devel \
        bison \
        ca-certificates \
        ccache \
        clang-15.0.7 \
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
        rsync \
        tar \
        unzip \
        valgrind \
        wget \
        which \
        bpftool \
    && dnf clean all

# Build dependencies from source
WORKDIR ${BUILD_DIR}

COPY install builder/install
COPY third_party third_party

RUN --mount=type=cache,target=/root/.ccache/ "builder/install/install-dependencies.sh" && \
    rm -rf ${BUILD_DIR} && \
    echo -e '/usr/local/lib\n/usr/local/lib64' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig

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
