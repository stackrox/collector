FROM golang:1.10.3-alpine3.8 as builder

COPY kobuild /go/src/github.com/stackrox/collector/kernel-modules/build/kobuild

WORKDIR /go/src/github.com/stackrox/collector/kernel-modules/build/kobuild

RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -a -o /tmp/kobuild .

FROM debian:jessie-slim

RUN apt-get update \
 && apt-get upgrade -y \
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
      # For CoreOS builds:
      bzip2 \
      kpartx \
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

COPY --from=builder /tmp/kobuild /usr/bin/

RUN mkdir -p /output
COPY build-kos /usr/bin/

WORKDIR /scratch
ENTRYPOINT [ "/usr/bin/kobuild" ]
