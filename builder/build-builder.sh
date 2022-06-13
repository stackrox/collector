
#ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
ROOT=$1

dnf -y update \
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
mkdir /install-tmp

cp -r "${ROOT}"/install/*.sh "/install-tmp"

# Build dependencies from source
"/install-tmp/install.sh"

echo '/usr/local/lib' >/etc/ld.so.conf.d/usrlocallib.conf && ldconfig

# Copy script for building collector
cp "${ROOT}/build/build-collector.sh" /
chmod 700 /build-collector.sh

# Set up ssh for remote development with IDE
ssh-keygen -A \
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
useradd -m remoteuser \
  && yes c0llectah | passwd remoteuser
￼
# Create directory to copy collector source into builder container
mkdir /src && chmod a+rwx /src
