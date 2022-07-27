FROM pipeline:src

COPY scripts /scripts

RUN whoami

# Temporary additional permissions to allow the OpenShift CI
# user to write to directories owned by the current 'circleci'
# user.
# TODO: adjust the base ci-image to allow reading/writing to these
#       directories.
RUN chmod -R o+w "$GOPATH" && \
    chmod -R o+w "$GOCACHE" && \
    chmod -R o+w "/go/pkg"

# The OpenShift CI user does not have a home directory
# despite $HOME being set. The SSH client will fail
# to resolve the home directory and will attempt to create
# /.ssh but cannot because of filesystem permissions.
#
# to work around this, we have to create the directory
# first.
RUN sudo mkdir /.ssh && sudo chmod o+w /.ssh
