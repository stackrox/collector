# Taken from https://github.com/pstauffer/docker-curl/blob/40794bf1d3958068eb32e30ce55d04c7a0d8991d/Dockerfile

FROM alpine:3.8

#
# Install all required dependencies.
#

RUN apk add --update curl && \
    rm -rf /var/cache/apk/*
