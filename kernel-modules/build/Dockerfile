FROM debian:stable

RUN apt-get update \
 && apt-get upgrade -y \
 && apt-get install --no-install-recommends -y \
      binutils \
      cmake \
      make \
      curl \
      gcc \
      gcc-7 \
      clang-7 \
      llvm-7 \
      g++ \
      libelf-dev \
      kmod \
      wget \
      golang-go \
      pkg-config \
 ;

RUN sed s_stable_jessie_g < /etc/apt/sources.list > /etc/apt/sources.list.d/jessie.list && \
    apt-get update && apt-get install --no-install-recommends -y \
      gcc-4.9 \
;

RUN ln -s /usr/bin/gcc-7 /usr/bin/gcc-6

RUN rm -rf /usr/bin/clang \
 && rm -rf /usr/bin/llc \
 && ln -s /usr/bin/clang-7 /usr/bin/clang \
 && ln -s /usr/bin/llc-7 /usr/bin/llc

RUN apt-get autoremove -y

# debian:unstable head contains binutils 2.31, which generates
# binaries that are incompatible with kernels < 4.16. So manually
# forcibly install binutils 2.30-22 instead.
RUN curl -s -o binutils_2.30-22_amd64.deb http://snapshot.debian.org/archive/debian/20180622T211149Z/pool/main/b/binutils/binutils_2.30-22_amd64.deb \
 && curl -s -o libbinutils_2.30-22_amd64.deb http://snapshot.debian.org/archive/debian/20180622T211149Z/pool/main/b/binutils/libbinutils_2.30-22_amd64.deb \
 && curl -s -o binutils-x86-64-linux-gnu_2.30-22_amd64.deb http://snapshot.debian.org/archive/debian/20180622T211149Z/pool/main/b/binutils/binutils-x86-64-linux-gnu_2.30-22_amd64.deb \
 && curl -s -o binutils-common_2.30-22_amd64.deb http://snapshot.debian.org/archive/debian/20180622T211149Z/pool/main/b/binutils/binutils-common_2.30-22_amd64.deb \
 && dpkg -i *binutils*.deb

RUN mkdir -p /output
COPY build-kos /usr/bin/
COPY prepare-src /usr/bin

WORKDIR /scratch
ENTRYPOINT [ "/bin/bash", "-c" ]
