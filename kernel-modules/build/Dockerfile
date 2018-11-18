FROM golang:1.10.3-alpine3.8 as builder

COPY kobuild /go/src/github.com/stackrox/collector/kernel-modules/build/kobuild

WORKDIR /go/src/github.com/stackrox/collector/kernel-modules/build/kobuild

RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -a -o /tmp/kobuild .

FROM debian:unstable-slim

RUN apt-get update \
 && apt-get upgrade -y \
 && apt-get install --no-install-recommends -y \
      binutils \
      cmake \
      make \
      gcc \
      g++ \
      libelf-dev \
      kmod \
      wget \
      `# For RedHat-family builds:` \
      rpm2cpio cpio \
      `# For CoreOS builds:` \
      bzip2 \
      kpartx \
 ;

RUN sed s_unstable_jessie_g < /etc/apt/sources.list > /etc/apt/sources.list.d/jessie.list && \
    apt-get update && apt-get install --no-install-recommends -y \
      gcc-4.9 \
;

RUN apt-get install --no-install-recommends -y \
    gcc-6 gcc-5 \
    ;


# Since our base Debian image ships with GCC 7 which breaks older kernels, revert the
# default to gcc-5.
RUN rm -rf /usr/bin/gcc && ln -s /usr/bin/gcc-5 /usr/bin/gcc

RUN rm -rf /usr/bin/clang \
 && rm -rf /usr/bin/llc \
 && ln -s /usr/bin/clang-7 /usr/bin/clang \
 && ln -s /usr/bin/llc-7 /usr/bin/llc

# debian:unstable head contains binutils 2.31, which generates
# binaries that are incompatible with kernels < 4.16. So manually
# forcibly install binutils 2.30-22 instead.
RUN apt-get install -y --no-install-recommends curl && curl -s -o binutils_2.30-22_amd64.deb http://snapshot.debian.org/archive/debian/20180622T211149Z/pool/main/b/binutils/binutils_2.30-22_amd64.deb \
 && curl -s -o libbinutils_2.30-22_amd64.deb http://snapshot.debian.org/archive/debian/20180622T211149Z/pool/main/b/binutils/libbinutils_2.30-22_amd64.deb \
 && curl -s -o binutils-x86-64-linux-gnu_2.30-22_amd64.deb http://snapshot.debian.org/archive/debian/20180622T211149Z/pool/main/b/binutils/binutils-x86-64-linux-gnu_2.30-22_amd64.deb \
 && curl -s -o binutils-common_2.30-22_amd64.deb http://snapshot.debian.org/archive/debian/20180622T211149Z/pool/main/b/binutils/binutils-common_2.30-22_amd64.deb \
 && dpkg -i *binutils*.deb

COPY --from=builder /tmp/kobuild /usr/bin/

RUN mkdir -p /output
COPY build-kos /usr/bin/
COPY prepare-src /usr/bin

WORKDIR /scratch
ENTRYPOINT [ "/usr/bin/kobuild" ]
