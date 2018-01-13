FROM debian:jessie-slim

RUN apt-get update \
 && apt-get install -y \
      binutils \
      cmake \
      make \
      gcc \
      g++ \
      # Some of the Debian builds require exactly g++-4.8, by name.
      gcc-4.8 \
      g++-4.8 \
      libelf-dev \
      kmod \
      wget \
      # For RedHat-family builds:
      rpm2cpio cpio \
 ;

# See https://github.com/draios/sysdig/blob/dev/docker/stable/Dockerfile.
# Quote:
#   Since our base Debian image ships with GCC 5.0 which breaks older kernels, revert the
#   default to gcc-4.9. Also, since some customers use some very old distributions whose kernel
#   makefile is hardcoded for gcc-4.6 or so (e.g. Debian Wheezy), we pretend to have gcc 4.6/4.7
#   by symlinking it to 4.9.
# We slightly modify this by only impersonating gcc 4.6 and 4.7, using 4.8.
RUN ln -s /usr/bin/gcc-4.8 /usr/bin/gcc-4.7 \
 && ln -s /usr/bin/gcc-4.8 /usr/bin/gcc-4.6 \
 ;

RUN mkdir -p /output
COPY sysdig/src /sysdig-src
COPY sysblock/src /sysblock-src
COPY build/build-kos /
WORKDIR /scratch
ENTRYPOINT [ "/build-kos" ]
