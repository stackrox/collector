FROM alpine:3.8

RUN echo http://dl-cdn.alpinelinux.org/alpine/edge/testing >> /etc/apk/repositories \
 && apk update \
 && apk add --no-cache \
    lbzip2=2.5-r1 \
    multipath-tools=0.7.7-r0 \
    pigz=2.4-r0 \
 && mkdir /tmp/mount

COPY repackage /usr/bin

ENTRYPOINT ["/usr/bin/repackage"]
CMD ["/input/coreos_developer_container.bin.bz2", "/output/bundle.tgz"]

