# Pinned to 3.18.4, which will mean that socat 1.7.4.4 is installed
# Updating to newer versions has caused test failures in socat tests
FROM alpine:3.18.4

RUN apk update && apk upgrade && apk add socat

ENTRYPOINT ["socat"]
