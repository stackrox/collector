FROM quay.io/stackrox-io/apollo-ci:collector-0.3.44-1-gb00ffc52af

COPY scripts /scripts

RUN whoami

RUN python3 -m pip install ansible

ENV GOCACHE="/linux-gocache"

# Temporary additional permissions to allow the OpenShift CI
# user to write to directories owned.
RUN mkdir -p "$GOCACHE" && \
    chmod -R g+rwx "$GOCACHE" && \
    chmod -R g+rwx "$GOPATH" && \
    chmod -R g+rwx "/go/pkg"

# The OpenShift CI user does not have a home directory
# despite $HOME being set. The SSH client will fail
# to resolve the home directory and will attempt to create
# /.ssh but cannot because of filesystem permissions.
#
# to work around this, we have to create the directory
# first.
RUN mkdir /.ssh && chmod g+rwx /.ssh

# Install hub-comment for commenting on PRs
RUN wget --quiet https://github.com/joshdk/hub-comment/releases/download/0.1.0-rc6/hub-comment_linux_amd64 && \
    install hub-comment_linux_amd64 /usr/bin/hub-comment
