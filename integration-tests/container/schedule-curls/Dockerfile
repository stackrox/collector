FROM alpine:3.8

RUN apk add --update curl && \
    apk add python3 && \
    rm -rf /var/cache/apk/*

COPY schedule-curls.py /usr/bin/schedule-curls.py
