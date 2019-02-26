FROM debian:stretch-slim

RUN apt-get update \
 && apt-get upgrade -y \
 && apt-get install --no-install-recommends -y \
      binutils \
      cmake \
      make \
      gcc \
      gcc-6 \
      g++ \
      libelf-dev \
      kmod \
      wget \
      `# For RedHat-family builds:` \
      rpm2cpio cpio \
      `# For CoreOS builds:` \
      bzip2 \
      kpartx \
      golang-go \
      pigz \
 ;

RUN sed s_stretch_jessie_g < /etc/apt/sources.list > /etc/apt/sources.list.d/jessie.list && \
    apt-get update && apt-get install --no-install-recommends -y \
      gcc-4.9 \
;

# Since our base Debian image ships with GCC 7 which breaks older kernels, revert the
# default to gcc-4.9
RUN rm -rf /usr/bin/gcc && ln -s /usr/bin/gcc-4.9 /usr/bin/gcc

RUN rm -rf /usr/bin/clang \
 && rm -rf /usr/bin/llc \
 && ln -s /usr/bin/clang-7 /usr/bin/clang \
 && ln -s /usr/bin/llc-7 /usr/bin/llc

RUN apt-get autoremove -y

RUN mkdir -p /output
COPY build-kos /usr/bin/
COPY prepare-src /usr/bin

WORKDIR /scratch

ENTRYPOINT ["/usr/bin/build-kos"]
