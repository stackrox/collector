FROM quay.io/centos/centos:stream9

ARG BUILD_DIR=/install-tmp

USER root

RUN dnf -y update \
    && dnf -y install --nobest \
        autoconf \
        automake \
        binutils-devel \
        bison \
        ca-certificates \
        clang-17.0.6 \
        llvm-17.0.6 \
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
        libubsan \
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
        # for USDT support
        systemtap-sdt-devel \
        rust \
        cargo \
    && dnf clean all

# Build dependencies from source
WORKDIR ${BUILD_DIR}

COPY install builder/install
COPY third_party third_party

RUN "builder/install/install-dependencies.sh" && \
    rm -rf ${BUILD_DIR} && \
    echo -e '/usr/local/lib\n/usr/local/lib64' > /etc/ld.so.conf.d/usrlocallib.conf && ldconfig

# Set up ssh for remote development with IDE
RUN ssh-keygen -A \
   && ( \
    echo 'LogLevel DEBUG2'; \
    echo 'PermitRootLogin yes'; \
    echo 'PasswordAuthentication yes'; \
    echo 'HostKeyAlgorithms ssh-ed25519'; \
    echo 'Subsystem sftp /usr/libexec/openssh/sftp-server'; \
  ) > /etc/ssh/sshd_config \
  && mkdir /run/sshd

# Add remote development user
RUN useradd -m remoteuser \
  && yes c0llectah | passwd remoteuser || if [[ $? -eq 141 ]]; then true; else exit $?; fi

# Create directory to copy collector source into builder container
RUN mkdir /src && chmod a+rwx /src

CMD ["/usr/sbin/sshd", "-D", "-e"]
