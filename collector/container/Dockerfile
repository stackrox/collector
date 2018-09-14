FROM stackrox/jq:bf88c73c20ff1642aa2566ae67df18ca5b4b63ab

FROM stackrox/kernel-modules:latest
MAINTAINER StackRox <support@stackrox.com>

# Add jq
COPY --from=0 /jq /usr/bin/jq

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
        kmod \
        libjsoncpp-dev \
        libssl-dev \
        wget \
        uuid-dev \
        libcap-ng0 \
 ;

COPY bin/curl /usr/local/bin/curl
COPY libs/libcurl.so \
     libs/libsinsp-wrapper.so \
     /usr/local/lib/

# `ldconfig` is required so that `curl` can find its `libcurl.so`.
RUN ldconfig -v

WORKDIR /

# Uncomment below when Address Sanitizer needs to be enabled for collector

# RUN apt-get update && apt-get install -y --no-install-recommends llvm && apt-get clean
# ENV ASAN_SYMBOLIZER_PATH /usr/bin/llvm-symbolizer-3.4
# ENV ASAN_OPTIONS symbolize=1:disable_core=0:abort_on_error=1:unmap_shadow_on_exit=1

# Uncomment below line when volume needs to created for extracting the Collector core
# VOLUME ["/core"]

# needed for container reentrancy on swarm:
RUN curl -o docker-1.13.0.tgz https://get.docker.com/builds/Linux/x86_64/docker-1.13.0.tgz && \
    tar -xvzf docker-1.13.0.tgz && cp docker/docker /usr/local/bin/docker && \
    rm -r docker-1.13.0.tgz docker
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

ENTRYPOINT ["/bootstrap.sh"]

# You must provide a BROKER_LIST environment variable or override the CMD.
# For information on other variables, see the explanations of these environment variables above.
CMD collector \
    --collector-config=$COLLECTOR_CONFIG \
    --chisel=$CHISEL \
    --broker-list=$BROKER_LIST \
    --grpc-server=$GRPC_SERVER

COPY bin/collector /usr/local/bin/collector
