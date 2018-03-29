#!/bin/bash

CONTAINER_ID=$(/usr/local/bin/docker inspect --format '{{.Id}}' $HOSTNAME)
NODE=$(/usr/local/bin/docker info -f '{{.Name}}')
NAME="${COLLECTOR_NAME}-${NODE}-priv"
IMAGE=$(/usr/local/bin/docker inspect --format '{{.Config.Image}}' $HOSTNAME)

cleanup() {
    /usr/local/bin/docker stop "$NAME"
}

trap cleanup EXIT

## In rare cases, the automatic garbage collection of the subcontainer
## that should happen because we use run --rm is too slow for us, so
## make sure any previous subcontainer is removed
EXISTING=$(docker ps -q -f name="$NAME")
if [ -n "$EXISTING" ]; then
    /usr/local/bin/docker rm -fv "$EXISTING" || true
fi

/usr/local/bin/docker run --privileged --rm --name $NAME \
        $COLLECTOR_ENVS --network=container:$CONTAINER_ID \
        $COLLECTOR_MOUNTS $COLLECTOR_LABELS \
        --log-driver='json-file' --log-opt='max-size=1m' --log-opt='max-file=10' \
        --cpu-period=100000 --cpu-quota=$ROX_COLLECTOR_CPU_QUOTA \
        --restart=no $IMAGE
