FROM alpine:3.9

RUN echo http://dl-cdn.alpinelinux.org/alpine/edge/testing >> /etc/apk/repositories \
 && apk update \
 && apk add --no-cache \
    lbzip2=2.5-r1 \
    multipath-tools=0.7.9-r0

COPY repackage /usr/bin

ENTRYPOINT ["/usr/bin/repackage"]
CMD ["/input/coreos_developer_container.bin.bz2", "/output"]
