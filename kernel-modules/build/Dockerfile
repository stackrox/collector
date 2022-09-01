FROM debian:stretch

RUN apt-get update \
 && apt-get upgrade -y \
 && apt-get install --no-install-recommends -y \
      binutils \
      git \
      ca-certificates \
      cmake \
      make \
      curl \
      gcc \
      clang-7 \
      llvm-7 \
      g++ \
      libelf-dev \
      kmod \
      wget \
      golang-go \
      pkg-config \
 ;

COPY third_party/googletest /googletest/
WORKDIR /googletest
RUN mkdir builddir && \
    cd builddir && \
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local .. && \
    cmake --build . --target install ${NPROCS:+-j ${NPROCS}}

RUN sed s_stretch_jessie_g < /etc/apt/sources.list > /etc/apt/sources.list.d/jessie.list && \
    apt-get update && apt-get install --no-install-recommends -y \
      gcc-4.9 \
;

# Since our base Debian image ships with GCC 6 which breaks older kernels, revert the
# default to gcc-4.9
RUN rm -rf /usr/bin/gcc && ln -s /usr/bin/gcc-4.9 /usr/bin/gcc
RUN rm -rf /usr/bin/clang \
 && rm -rf /usr/bin/llc \
 && ln -s /usr/bin/clang-7 /usr/bin/clang \
 && ln -s /usr/bin/llc-7 /usr/bin/llc

RUN apt-get autoremove -y

# Ensure binutils is less than 2.31, which is incompatiable with kernels < 4.16
RUN dpkg --compare-versions $(dpkg-query -f='${Version}' --show binutils) lt 2.31

RUN mkdir -p /output
COPY kernel-modules/build/build-kos /scripts/
COPY kernel-modules/build/build-wrapper.sh /usr/bin/
COPY kernel-modules/build/prepare-src /usr/bin

WORKDIR /scratch
ENTRYPOINT [ "/bin/bash", "-c" ]
