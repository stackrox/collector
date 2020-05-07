FROM debian:buster-slim

ARG collector_version=xxx
ARG module_version=xxx

LABEL maintainer="StackRox <support@stackrox.com>"
LABEL io.stackrox.collector.module-version="${module_version}"

ENV MODULE_VERSION=${module_version}

# Add module version file only
RUN mkdir /kernel-modules && \
    echo "${module_version}" >/kernel-modules/MODULE_VERSION.txt

ENV COLLECTOR_HOST_ROOT=/host

# HTTP API port (8080)
EXPOSE 8080 9090

RUN apt-get update \
 && apt-get upgrade -y \
 && apt-get install -y --no-install-recommends \
        ca-certificates \
        kmod \
        curl \
        libssl1.1 \
        libuuid1 \
        libcap-ng0 \
        libelf1 \
        google-perftools \
        `# https://security-tracker.debian.org/tracker/CVE-2017-14062` \
        libidn11=1.33-2.2 \
 && dpkg -r --force-all e2fslibs e2fsprogs apt debconf dpkg \
 && rm -rf /var/lib/apt \
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

# update boot script
COPY scripts/bootstrap.sh /bootstrap.sh
COPY scripts/collector-wrapper.sh /usr/local/bin/
RUN chown root.root /bootstrap.sh
RUN chmod 700 /bootstrap.sh

# copy notice and license files
COPY NOTICE-collector.txt \
     COPYING.txt \
     /

ENTRYPOINT ["/bootstrap.sh"]

CMD collector-wrapper.sh \
    --collector-config=$COLLECTOR_CONFIG \
    --collection-method=$COLLECTION_METHOD \
    --grpc-server=$GRPC_SERVER

COPY bin/collector /usr/local/bin/

LABEL io.stackrox.collector.version="${collector_version}"
ENV COLLECTOR_VERSION=${collector_version}
