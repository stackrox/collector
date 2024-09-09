FROM alpine:3.20

RUN apk add --update curl && \
    rm -rf /var/cache/apk/*

COPY schedule-curls.sh /usr/bin/schedule-curls.sh 
