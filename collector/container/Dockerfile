ARG base=latest
# This is a hack because the argument to FROM must be either a hardcoded string or a variable expansion, but not a
# mix of both.
ARG base_image=stackrox/kernel-modules:${base}

FROM ${base_image} AS kernel_modules

FROM debian:stretch-slim

MAINTAINER StackRox <support@stackrox.com>

# Add kernel modules
COPY --from=kernel_modules /kernel-modules /kernel-modules

# SYSDIG_HOST_ROOT is required by the sysdig kernel module; used as a prefix to
# paths like /dev/sysdig0, /proc, and /boot. Make sure this matches the volume
# mount paths used when launching this container.
#
# COLLECTOR_CONFIG is the required json config string, and it defaults to
# { "syscalls": [],"topic_query_field": "" }
#
# MAX_CONTENT_LENGTH_KB is optional, and it defaults to 1024.
#
# CONNECTION_LIMIT is optional, and it defaults to 64.
#
# CONNECTION_LIMIT_PER_IP is optional, and it defaults to 64.
#
# CONNECTION_TIMEOUT is optional, and it defaults to 8.
#
ENV SYSDIG_HOST_ROOT=/host \
    MAX_CONTENT_LENGTH_KB=1024 \
    CONNECTION_LIMIT=64 \
    CONNECTION_LIMIT_PER_IP=64 \
    CONNECTION_TIMEOUT=8 \
    CHISEL='LS0gQ2hpc2VsIGRlc2NyaXB0aW9uCmRlc2NyaXB0aW9uID0gIm9ubHkgZGlzcGxheSBldmVudHMgcmVsZXZhbnQgdG8gc2VjdXJpdHkgbW9kZWxpbmcgYW5kIGRldGVjdGlvbiIKc2hvcnRfZGVzY3JpcHRpb24gPSAic2VjdXJpdHkgcmVsZXZhbnQiCmNhdGVnb3J5ID0gIm1pc2MiCgotLSBDaGlzZWwgYXJndW1lbnQgbGlzdAphcmdzID0ge30KCi0tIEV2ZW50IHBhcnNpbmcgY2FsbGJhY2sKZnVuY3Rpb24gb25fZXZlbnQoKQogICAgcmV0dXJuIHRydWUKZW5kCgpmdW5jdGlvbiBvbl9pbml0KCkKICAgIGZpbHRlciA9ICJub3QgY29udGFpbmVyLmlkID0gJ2hvc3QnXG4iCiAgICBjaGlzZWwuc2V0X2ZpbHRlcihmaWx0ZXIpCiAgICByZXR1cm4gdHJ1ZQplbmQK'

# HTTP API port (8080)
EXPOSE 8080 9090

RUN apt-get update \
 && apt-get upgrade -y \
 && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        libcurl3 \
        openssl \
        libuuid1 \
        libcap-ng0 \
        jq \
        libluajit-5.1 \
        upx-ucl \
 ;

COPY libs/libsinsp-wrapper.so \
     /usr/local/lib/

WORKDIR /

# Uncomment below when Address Sanitizer needs to be enabled for collector

# RUN apt-get update && apt-get install -y --no-install-recommends llvm && apt-get clean
# ENV ASAN_SYMBOLIZER_PATH /usr/bin/llvm-symbolizer-3.4
# ENV ASAN_OPTIONS symbolize=1:disable_core=0:abort_on_error=1:unmap_shadow_on_exit=1

# Uncomment below line when volume needs to created for extracting the Collector core
# VOLUME ["/core"]

# needed for container reentrancy on swarm:
RUN curl -o docker-18.06.1-ce.tgz https://download.docker.com/linux/static/stable/x86_64/docker-18.06.1-ce.tgz && \
    tar -xvzf docker-18.06.1-ce.tgz && cp docker/docker /usr/local/bin/docker && \
    rm -r docker-18.06.1-ce.tgz docker && \
    upx -9 /usr/local/bin/docker
COPY scripts/swarm_entrypoint.sh /swarm_entrypoint.sh

# update boot script
COPY scripts/bootstrap.sh /bootstrap.sh
RUN chown root.root /bootstrap.sh
RUN chmod 700 /bootstrap.sh

COPY scripts/extract_secrets.sh /extract_secrets.sh
RUN chown root.root /extract_secrets.sh
RUN chmod 700 /extract_secrets.sh

# copy notice and license files
COPY NOTICE-collector.txt \
     COPYING.txt \
     /

COPY bin/ /usr/local/bin/

RUN upx /usr/local/bin/collector

RUN apt-get remove -y upx-ucl && apt-get -y autoremove

ENTRYPOINT ["/bootstrap.sh"]

# You must provide a BROKER_LIST environment variable or override the CMD.
# For information on other variables, see the explanations of these environment variables above.
CMD collector \
    --collector-config=$COLLECTOR_CONFIG \
    --chisel=$CHISEL \
    --broker-list=$BROKER_LIST \
    --grpc-server=$GRPC_SERVER
