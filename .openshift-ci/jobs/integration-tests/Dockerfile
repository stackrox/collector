FROM pipeline:src

COPY scripts /scripts

RUN whoami

RUN chmod -R o+w "$GOPATH" && \
    chmod -R o+w "$GOCACHE" && \
    chmod -R o+w "/go/pkg"

